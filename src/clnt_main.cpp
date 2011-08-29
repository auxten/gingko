/**
 *  gingko_clnt.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-10.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/
#ifndef GINGKO_CLNT
#define GINGKO_CLNT
#endif /** GINGKO_CLNT **/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <getopt.h>
#include <inttypes.h>
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /** __APPLE__ **/

#include "gingko.h"
#include "async_pool.h"
#include "hash/xor_hash.h"
#include "path.h"
#include "route.h"
#include "log.h"
#include "snap.h"
#include "option.h"
#include "socket.h"
#include "limit.h"
#include "job_state.h"
#include "gingko_clnt.h"

/************** PTHREAD STUFF **************/
///default pthread_attr_t
pthread_attr_t g_attr;
///client wide lock
pthread_mutex_t g_clnt_lock;
///block host set lock
pthread_mutex_t g_blk_hostset_lock;
///mutex for gko.hosts_new_noready
pthread_mutex_t g_hosts_new_noready_mutex;
///mutex for gko.hosts_del_noready
pthread_mutex_t g_hosts_del_noready_mutex;
/************** PTHREAD STUFF **************/

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT
GINGKO_OVERLOAD_S_HOST_EQ

using namespace std;


/// the g_job assoiate with the client
s_job_t g_job;

/// gingko global stuff
s_gingko_global_t gko;


/**
 * @brief download the vnode area
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * vnode_download(void * arg)
{
    s_vnode_download_arg_t *p = (s_vnode_download_arg_t *) arg;
    s_job_t * jo = p->jo;
    GKO_INT64 blk_idx = p->blk_idx;
    GKO_INT64 blk_count = p->blk_count;
    vector<s_host_t> h_vec;
    s_host_t h;
    GKO_INT64 i, tmp, blk_got = 0;
    char * buf;
    int retry = 0;
    /**
     * prepare the buf to readall data
     **/
    if ((buf = (char *) malloc(BLOCK_SIZE)) == NULL)
    {
        gko_log(FATAL, "malloc for read buf of blocks_size failed");
        pthread_exit((void *) 3);
    }
    while (blk_got < blk_count && retry <= MAX_RETRY)
    {
        GKO_INT64 blk_idx_tmp;
        char work_done_flag = 0;
        while ((g_job.blocks + (blk_idx + blk_got) % jo->block_count)->done)
        {
            if (++blk_got > blk_count)
            {
                work_done_flag = 1;
                break;
            }
        }
        if (work_done_flag)
        {
            break;
        }
        blk_idx_tmp = (blk_idx + blk_got) % jo->block_count;
        i = get_blk_src(jo, FIRST_HOST_COUNT, blk_idx_tmp, &h_vec);
        i = decide_src(jo, SECOND_HOST_COUNT, blk_idx_tmp, &h_vec, &h, buf);
        if (UNLIKELY(! i))
        { /** got no available src **/
            gko_log(WARNING, "decide_src ret: %lld", i);
            sleep(++retry);
            if (retry > MAX_RETRY)
            {
                pthread_exit((void *) 2);
            }
        }
        else
        {
            retry = 0;
        }
        /**
         *  if intend to get_blocks_c from gko.the_serv
         *	just request no more than MAX_REQ_SERV_BLOCKS
         **/
        ///find the block non-done edge
        GKO_INT64 count2edge = 0;
        while (!(g_job.blocks + (blk_idx + blk_got + i + count2edge + 1)
                % g_job.block_count)->done)
        {
            if (++count2edge >= blk_count - blk_got)
            {
                break;
            }
        }
        GKO_INT64 blk_cnt_tmp;
        if (h == gko.the_serv)
        {
            blk_cnt_tmp
            =
                MIN(MIN(MAX_REQ_SERV_BLOCKS, blk_count - blk_got - i), count2edge);
        }
        else
        {
            blk_cnt_tmp = MIN(blk_count - blk_got - i, count2edge);
        }
        tmp = get_blocks_c(jo, &h, (blk_idx + blk_got + i) % jo->block_count,
                blk_cnt_tmp, 0 | W_DISK, buf);
        if (LIKELY(tmp >= 0))
        {
            blk_got += tmp;
            blk_got += i;
        }
        else
        {
            gko_log(WARNING, "get_blocks_c ret: %lld", tmp);
        }
        ///printf("get_blocks_c: %lld", tmp);
    }
    free(buf);
    buf = NULL;
    pthread_exit((void *) 0);
}

/**
 * @brief download all nodes area
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int node_download(void *)
{
    pthread_t vnode_pthread[VNODE_NUM];
    void *status;
    if (!g_job.block_count)
    {
        return 0;
    }
    s_host_hash_result_t h_hash;
    ///find out the initial block zone, save it in h_hash struct
    host_hash(&g_job, &gko.the_clnt, &h_hash, ADD_HOST);
    ///find incharge block count
    for (int i = 0; i < VNODE_NUM; i++)
    {
        GKO_INT64 j;
        j = h_hash.v_node[i];
        if (j == -1)
        {
            h_hash.length[i] = 0;
        }
        else
        {
            h_hash.length[i] = 1;
            /**
             * Calculate the vnode area length
             * go back until we find the node has gko.the_clnt
             **/
            set<s_host_t> * host_set_p =
                    (g_job.blocks + (j + 1) % g_job.block_count)->host_set;
            while (!host_set_p || (*host_set_p).find(gko.the_clnt)
                    == (*host_set_p).end())
            {

                ///printf("j: %lld", j%g_job.block_count);
                h_hash.length[i]++;
                host_set_p = (g_job.blocks + (++j + 1) % g_job.block_count)->host_set;
            }
        }
        gko_log(NOTICE, "vnode%d: %lld, length: %lld", i, h_hash.v_node[i],
                h_hash.length[i]);
    }
    s_vnode_download_arg_t vnode_arg[VNODE_NUM];
    for (int i = 0; i < VNODE_NUM; i++)
    {
        vnode_arg[i].jo = &g_job;
        vnode_arg[i].blk_idx = h_hash.v_node[i];
        vnode_arg[i].blk_count = h_hash.length[i];
        pthread_create(&vnode_pthread[i], &g_attr, vnode_download, &vnode_arg[i]);
    }
    for (int i = 0; i < VNODE_NUM; i++)
    {
        pthread_join(vnode_pthread[i], &status);
        if (status != (void *) 0)
        {
            gko_log(FATAL, "thread %d joined with error num %lld", i,
                    (long long) status);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief the thread worker for download the other one is upload
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * downloadworker(void *)
{
    {
        /**
         * get the public ip by sending HELO to serv
         **/
        if (helo_serv_c(NULL, 0, &gko.the_serv))
        {
            gko_log(FATAL, "send HELO to server failed!!");
            pthread_exit((void *) -1);
        }
    }

    {
        /**
         * wait until the server thread listen a port
         **/
        while (! gko.the_clnt.port)
        {
            usleep(100);
        }
    }

    {
        /**
         * join g_job
         * and reset all the done flag cause the server's done are all 1
         **/
        if ( join_job_c((void *) g_job.uri, 0))
        {
            gko_log(FATAL, "join g_job error!!");
            pthread_exit((void *) -1);
        }
        for (int i = 0; i < g_job.block_count; i++)
        {
            (g_job.blocks + i)->done = 0;
        }
    }

    {
        /**
         * process the path according to the src and dest
         **/
        if (process_path(&g_job))
        {
            gko_log(FATAL, "convert path to local failed");
            pthread_exit((void *) -1);
        }
    }

    {
        /**
         * mk dirs and soft links, trancate files
         * if to_continue check the existed file size
         **/
        if ( mk_dir_symlink_file(&g_job, &gko.opt.to_continue))
        {
            gko_log(FATAL, "make dir symlink file failed");
            pthread_exit((void *) -1);
        }
        ///ready to serv !!, no need for mutex here
        gko.ready_to_serv = 1;
    }

    {
        /**
         *  generate the gingko snap file path, this should be done after
         *  all dir is made
         **/
        if (!gen_snap_fpath(gko.snap_fpath, g_job.path, g_job.uri))
        {
            gko_log(FATAL, FLF("gen snap path error"));
            pthread_exit((void *) -1);
        }
    }

    {
        /**
         * continue download logic
         **/
        if (gko.opt.to_continue)
        {
            if (FAIL_CHECK(load_snap(&g_job)))
            {
                gko_log(WARNING, "load snap file failed");
            }
        }
        /**
         * reopen the snap file, O_TRUNC is in flag, write the last blk snap
         * to make the file size fit to the g_job.block_count * BLOCK_SIZE
         **/
        close(gko.snap_fd);
        if ((gko.snap_fd = open(gko.snap_fpath, SNAP_REOPEN_FLAG,
                SNAP_FILE_MODE)) == -1)
        {
            gko_log(FATAL, "make the snap file failed");
            pthread_exit((void *) -1);
        }
        if (FAIL_CHECK(ftruncate(gko.snap_fd,
                        g_job.block_count * sizeof(s_snap_t))))
        {
            gko_log(WARNING, "first pwrite snap failed");
        }
    }

    {
        /**
         * insert and host_hash the hosts NEWWed before gko.ready_to_serv
         **/
        pthread_mutex_lock(&g_hosts_new_noready_mutex);
        pthread_mutex_lock(&g_clnt_lock);
        (*(g_job.host_set)).insert(gko.hosts_new_noready.begin(),
                gko.hosts_new_noready.end());
        pthread_mutex_unlock(&g_clnt_lock);

        for (vector<s_host_t>::iterator it = gko.hosts_new_noready.begin();
                it != gko.hosts_new_noready.end(); it++)
        {
            host_hash(&g_job, &(*it), NULL, ADD_HOST);
        }
        pthread_mutex_unlock(&g_hosts_new_noready_mutex);
    }

    {
        /**
         * delete the hosts DELEed before gko.ready_to_serv
         **/
        pthread_mutex_lock(&g_hosts_del_noready_mutex);
        pthread_mutex_lock(&g_clnt_lock);
        for (vector<s_host_t>::iterator it = gko.hosts_del_noready.begin(); it
                != gko.hosts_del_noready.end(); it++)
        {
            (*(g_job.host_set)).erase(*it);
        }
        pthread_mutex_unlock(&g_clnt_lock);

        for (vector<s_host_t>::iterator it = gko.hosts_del_noready.begin(); it
                != gko.hosts_del_noready.end(); it++)
        {
            host_hash(&g_job, &(*it), NULL, DEL_HOST);
        }
        pthread_mutex_unlock(&g_hosts_del_noready_mutex);
    }

    {
        /**
         * Start download NOW!!
         **/
        if(node_download(NULL))
        {
            gko_log(FATAL, "download data failed");
            pthread_exit((void *) -1);
        }
    }

    gko_log(NOTICE,
            "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@node_download done");
    fprintf(stderr, "all data download success, uploading...\n");
    for (GKO_INT64 i = 0; i < g_job.block_count; i++)
    {
        if ((g_job.blocks + i)->done != 1)
        {
            gko_log(NOTICE, "undone: %lld", i);
        }
    }
    pthread_exit((void *) 0);
}

/**
 * @brief init the pthread stuff
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC inline void pthread_init()
{
    pthread_attr_init(&g_attr);
    pthread_attr_setdetachstate(&g_attr, PTHREAD_CREATE_JOINABLE);
    pthread_mutex_init(&g_clnt_lock, NULL);
    pthread_mutex_init(&g_blk_hostset_lock, NULL);
    pthread_mutex_init(&g_hosts_new_noready_mutex, NULL);
    pthread_mutex_init(&g_hosts_del_noready_mutex, NULL);
    return;
}

/**
 * @brief clean the pthread stuff
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC inline void pthread_clean()
{
    pthread_attr_destroy(&g_attr);
    pthread_mutex_destroy(&g_clnt_lock);
    pthread_mutex_destroy(&g_blk_hostset_lock);
    pthread_mutex_destroy(&g_hosts_new_noready_mutex);
    pthread_mutex_destroy(&g_hosts_del_noready_mutex);
    return;
}

/**
 * @brief INT handler for client, quit the g_job elegantly
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void clnt_int_handler(const int sig)
{
    gko.sig_flag = sig;
}

/**
 * @brief this thread will watch the gko.sig_flag and take action
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-9
 **/
GKO_STATIC_FUNC void * clnt_int_worker(void * a)
{
    while(1)
    {
        if (UNLIKELY(gko.sig_flag))
        {
            /**
             * send QUIT cmd to server
             **/
            quit_job_c(&gko.the_clnt, &gko.the_serv, g_job.uri);
            /// Clear all status
            gko_log(WARNING, "Client terminated.");
            exit(1);
        }
        usleep(CK_SIG_INTERVAL);
    }
}

/**
 * @brief init the global stuff, also the args
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int gingko_clnt_global_init(int argc, char *argv[])
{
    memset(&g_job, 0, sizeof(g_job));
    memset(&gko, 0, sizeof(gko));
    memset(&gko.the_clnt, 0, sizeof(gko.the_clnt));
    memset(&gko.the_serv, 0, sizeof(gko.the_serv));

    if(clnt_parse_opt(argc, argv, &g_job) == 0)
    {
        gko_log(NOTICE, "opts parsed successfully");
    }
    else
    {
        return -1;
    }

    set_sig(clnt_int_handler);
    umask(0);

    /**
     * continue init global vars stuff
     **/
    gko.ready_to_serv = 0;
    gko.cmd_list_p = g_cmd_list;
    gko.func_list_p = g_func_list_s;
    gko.snap_fd = -2;
    gko.sig_flag = 0;

    return 0;
}

/**
 * @brief main
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int main(int argc, char *argv[])
{
    pthread_t download, upload;
    void *status;
    if(gingko_clnt_global_init(argc, argv) != 0)
    {
        gko_log(FATAL, "gingko_clnt_global_init failed");
        exit(1);
    }
    pthread_init();
    s_async_server_arg_t serv_arg;
    serv_arg.s_host_p = &gko.the_clnt;
    /// start check the sig_flag
    if (sig_watcher(clnt_int_worker))
    {
        gko_log(FATAL, "signal watcher start error");
        exit(1);
    }

    pthread_create(&download, &g_attr, downloadworker, NULL);
    pthread_create(&upload, &g_attr, gingko_clnt_async_server,
            (void *) (&serv_arg));
    pthread_join(download, &status);
    if (status != (void *)0)
    {
        gko_log(FATAL, "download failed, quiting");
        quit_job_c(&gko.the_clnt, &gko.the_serv, g_job.uri);
        exit(1);
    }
    else
    {
        if (correct_mode(&g_job))
        {
            gko_log(FATAL, "correct_mode failed");
            quit_job_c(&gko.the_clnt, &gko.the_serv, g_job.uri);
            exit(1);
        }
        sleep(gko.opt.seed_time);
    }
    quit_job_c(&gko.the_clnt, &gko.the_serv, g_job.uri);
    gko_log(NOTICE, "upload end, client quited");
    fprintf(stderr, "upload end, client quited\n");
    pthread_clean();
    exit(0);
}
