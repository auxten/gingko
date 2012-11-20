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
#include <sys/time.h>
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
#elif defined (__FreeBSD__)
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>
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
#include "progress.h"
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


/// the g_job assoiate with the client
s_job_t g_job;

/// gingko global stuff
s_gingko_global_t gko;


/**
 * @brief download the vnode area
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * vnode_download(void * arg)
{
    s_vnode_download_arg_t *p = (s_vnode_download_arg_t *) arg;
    s_job_t * jo = p->jo;
    GKO_INT64 blk_idx = p->blk_idx;
    GKO_INT64 blk_count = p->blk_count;
    std::vector<s_host_t> h_vec;
    s_host_t h;
    GKO_INT64 i;
    GKO_INT64 tmp;
    GKO_INT64 blk_got = 0;
    GKO_INT64 last_blk_got = 0;

    char * buf;
    int retry = 0;
    /**
     * prepare the buf to readall data
     **/
    buf = new char[BLOCK_SIZE + UNZIP_EXTRA];
    if (buf == NULL)
    {
        gko_log(FATAL, "new for read buf of blocks_size failed");
        pthread_exit((void *) 3);
    }
    while (blk_got < blk_count && retry <= MAX_DOWN_RETRY)
    {
        GKO_INT64 blk_idx_tmp;
        char work_done_flag = 0;

        if (gko.opt.need_progress)
        {
            show_progress(blk_got - last_blk_got);
            last_blk_got = blk_got;
        }

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

        get_blk_src(jo, FIRST_HOST_COUNT + retry, blk_idx_tmp, &h_vec);

        i = decide_src(jo, SECOND_HOST_COUNT + retry, blk_idx_tmp, &h_vec, &h, buf);
        if (UNLIKELY(! i))
        { /** got no available src **/
            gko_log(WARNING, "decide_src ret: %lld", i);
            sleep(1 + (++retry) / 5 );
            if (retry > MAX_DOWN_RETRY)
            {
                delete [] buf;
                buf = NULL;
                pthread_exit((void *) 2);
            }
            else
            {
                continue;
            }
        }
        else
        {
            blk_got += i;
            retry /= SUCC_RETRY_DIV;
        }
        /**
         *  if intend to get_blocks_c from gko.the_serv
         *	just request no more than MAX_REQ_SERV_BLOCKS
         **/
        ///find the block non-done edge
        GKO_INT64 count2edge = 0;
        while (!(g_job.blocks + (blk_idx + blk_got + count2edge + 1)
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
                MIN(MIN(MAX_REQ_SERV_BLOCKS+retry, blk_count-blk_got), count2edge);
        }
        else
        {
            blk_cnt_tmp = MIN(MIN(blk_count - blk_got, count2edge), MAX_REQ_CLNT_BLOCKS+retry);
        }
        tmp = get_blocks_c(jo, &h, (blk_idx + blk_got) % jo->block_count,
                blk_cnt_tmp, 0 | W_DISK, buf);
        if (LIKELY(tmp >= 0))
        {
            blk_got += tmp;
        }
        else
        {
            gko_log(WARNING, "get_blocks_c ret: %lld", tmp);
        }
        ///printf("get_blocks_c: %lld", tmp);
    }
    /// show the 100%
    if (gko.opt.need_progress)
    {
        show_progress(blk_got - last_blk_got);
        last_blk_got = blk_got;
    }

    delete [] buf;
    buf = NULL;
    pthread_exit((void *) 0);
}

/**
 * @brief download all nodes area
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
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
    host_hash(&g_job, &gko_pool::gko_serv, &h_hash, ADD_HOST);
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
             * go back until we find the node has gko_pool::gko_serv
             **/
            std::set<s_host_t> * host_set_p =
                    (g_job.blocks + (j + 1) % g_job.block_count)->host_set;
            while (!host_set_p || (*host_set_p).find(gko_pool::gko_serv)
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
        if (pthread_create(&vnode_pthread[i], &g_attr, vnode_download, &vnode_arg[i]))
        {
            gko_log(FATAL, "download thread %d create error", i);
            return -1;
        }
    }
    for (int i = 0; i < VNODE_NUM; i++)
    {
        if (pthread_join(vnode_pthread[i], &status))
        {
            gko_log(FATAL, "download thread %d join error", i);
            return -1;
        }
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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * downloadworker(void *)
{
    {
        /**
         * get the public ip by sending HELO to serv
         **/
        int helo_ret;
        int helo_retry = 0;
        do
        {
            helo_ret = helo_serv_c(NULL, 0, &gko.the_serv);
            if (helo_ret == 0)
            {
                break;
            }
            else if (helo_ret == HOST_DOWN_FAIL)
            {
                gko_log(FATAL,
                        "send HELO to server failed, server may be down");
                pthread_exit((void *) -1);
            }
            else
            {
                sleep(HELO_RETRY_INTERVAL);
                helo_retry++;
                if (helo_retry >= MAX_HELO_RETRY)
                {
                    gko_log(FATAL, "send HELO to server failed!!");
                    pthread_exit((void *) -1);
                }
            }
        } while (helo_ret != 0);
    }

    {
        /**
         * wait until the server thread listen a port
         **/
        while (! gko_pool::gko_serv.port)
        {
            usleep(100000);
        }
    }

    /// sleep for some time for make clients req server at distribute time
    usleep(xor_hash(&gko_pool::gko_serv, sizeof(gko_pool::gko_serv), 0) % 5000000);

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
        else
        {
            g_job.job_state = JOB_IS_JOINED;
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
    }

    {
        /*
         * tell all the clients I know: "NEWW ip port"
         */
        char broadcast_buf[SHORT_MSG_LEN];
        memset(broadcast_buf, 0, SHORT_MSG_LEN);
        snprintf(broadcast_buf, SHORT_MSG_LEN, "NEWW\t%s\t%d",
                gko_pool::gko_serv.addr, gko_pool::gko_serv.port);
        for (std::set<s_host_t>::const_iterator i = (*(g_job.host_set)).begin(); i
                != (*(g_job.host_set)).end(); i++)
        {
            sendcmd2host((s_host_t *) &(*i), broadcast_buf, 2, 2);
        }
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
            gko_log(WARNING, "ftruncate snap failed");
        }
    }

    {
        /**
         * ready to serv !!, no need for mutex here
         */
        gko.ready_to_serv = 1;
    }

    {
        /**
         * insert and host_hash the hosts NEWWed before gko.ready_to_serv
         **/
        pthread_mutex_lock(&g_hosts_new_noready_mutex);
        pthread_mutex_lock(&g_clnt_lock);
        (*(g_job.host_set)).insert(
                gko.hosts_new_noready.begin(),
                gko.hosts_new_noready.end());
        update_host_max(&g_job);
        pthread_mutex_unlock(&g_clnt_lock);

        for (std::vector<s_host_t>::const_iterator it = gko.hosts_new_noready.begin();
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
        for (std::vector<s_host_t>::const_iterator it = gko.hosts_del_noready.begin(); it
                != gko.hosts_del_noready.end(); it++)
        {
            (*(g_job.host_set)).erase(*it);
        }
        pthread_mutex_unlock(&g_clnt_lock);

        for (std::vector<s_host_t>::const_iterator it = gko.hosts_del_noready.begin(); it
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
        struct timeval dl_start_time;
        struct timeval dl_end_time;
        gettimeofday(&dl_start_time, NULL);
        if(node_download(NULL))
        {
            gko_log(FATAL, "download data failed");
            pthread_exit((void *) -1);
        }
        gettimeofday(&dl_end_time, NULL);
        timersub(&dl_end_time, &dl_start_time, &g_job.dl_time); /// time used for download
    }

    if (gko.opt.need_progress)
    {
        putchar('\n');
    }

    char alldone = 1;
    for (GKO_INT64 i = 0; i < g_job.block_count; i++)
    {
        if ((g_job.blocks + i)->done != 1)
        {
            alldone = 0;
            gko_log(NOTICE, "undone: %lld", i);
        }
    }

    if(alldone)
    {
        /*
         * if all ends successfully, remove the useless snap file
         */
        if (unlink(gko.snap_fpath) == 0)
        {
            gko_log(DEBUG, "snap file removed succ");
        }
        else
        {
            gko_log(NOTICE, "snap file remove failed");
        }
        gko_log(NOTICE,
                "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@node_download done");
        fprintf(stderr, "all data download success, uploading...\n");
        pthread_exit((void *) 0);
    }
    else
    {
        gko_log(FATAL,
                "some block missed");
        fprintf(stderr, "data download fail, some block missed\n");
        pthread_exit((void *) 1);
    }
}

/**
 * @brief init the pthread stuff
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int pthread_init()
{
    if (pthread_attr_init(&g_attr) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_attr_setdetachstate(&g_attr, PTHREAD_CREATE_JOINABLE) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_init(&g_clnt_lock, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_init(&g_blk_hostset_lock, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_init(&g_hosts_new_noready_mutex, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_init(&g_hosts_del_noready_mutex, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_destroy error"));
        return -1;
    }

    return 0;
}

/**
 * @brief clean the pthread stuff
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int pthread_clean()
{
    if (pthread_attr_destroy(&g_attr) != 0)
    {
        gko_log(WARNING, FLF("pthread_attr_destroy error"));
        return -1;
    }
    if (pthread_mutex_destroy(&g_clnt_lock) != 0)
    {
        gko_log(WARNING, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_destroy(&g_blk_hostset_lock) != 0)
    {
        gko_log(WARNING, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_destroy(&g_hosts_new_noready_mutex) != 0)
    {
        gko_log(WARNING, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    if (pthread_mutex_destroy(&g_hosts_del_noready_mutex) != 0)
    {
        gko_log(WARNING, FLF("pthread_mutex_destroy error"));
        return -1;
    }
    return 0;
}

/**
 * @brief INT handler for client, quit the g_job elegantly
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
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
 * @author auxten  <auxtenwpc@gmail.com>
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
            quit_job_c(&gko_pool::gko_serv, &gko.the_serv, g_job.uri);
            /// Clear all status
            gko_log(WARNING, "Client terminated.");
            gko_quit(1);
        }
        usleep(CK_SIG_INTERVAL);
    }
}

/**
 * @brief init the global stuff, also the args
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int gingko_clnt_global_init(int argc, char *argv[])
{
    memset(&g_job, 0, sizeof(g_job));
    memset(&gko, 0, sizeof(gko));
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
    gko.snap_fd = -2;
    gko.sig_flag = 0;

    return 0;
}

/**
 * @brief client side async server starter
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * gingko_clnt_async_server(void * arg)
{
    gko_pool * gingko = gko_pool::getInstance();
    gingko->setPort(RANDOM_PORT); // -1 for random port
    gingko->setOption(&gko.opt);
    gingko->setFuncTable(g_cmd_list, g_func_list_s, CMD_COUNT);
    pthread_exit((void *) gingko->gko_run());
}


/**
 * @brief main
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int main(int argc, char *argv[])
{
    /// download pthread
    pthread_t download;
    /// upload pthread
    pthread_t upload;
    void *status;
    if(gingko_clnt_global_init(argc, argv) != 0)
    {
        gko_log(FATAL, "gingko_clnt_global_init failed");
        gko_quit(1);
    }

    gko_log(DEBUG, "Debug mode start, i will print tons of log :p!");

    if (pthread_init() != 0)
    {
        gko_log(FATAL, FLF("pthread_init error"));
        fprintf(stderr, "Client error, quited\n");
        gko_quit(1);
    }
    /// start check the sig_flag
    if (sig_watcher(clnt_int_worker) != 0)
    {
        gko_log(FATAL, "signal watcher start error");//todo
        gko_quit(1);
    }

    if (pthread_create(&download, &g_attr, downloadworker, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_create error"));
        gko_quit(1);
    }
    if (pthread_create(&upload, &g_attr, gingko_clnt_async_server, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_create error"));
        gko_quit(1);
    }
    if (pthread_join(download, &status) != 0)
    {
        gko_log(FATAL, FLF("pthread_join error"));
        gko_quit(1);
    }
    if (status != (void *)0)
    {
        gko_log(FATAL, "download failed, quiting");
        quit_job_c(&gko_pool::gko_serv, &gko.the_serv, g_job.uri);
        gko_quit(1);
    }
    else
    {
        if (correct_mode(&g_job))
        {
            gko_log(FATAL, "correct_mode failed");
            quit_job_c(&gko_pool::gko_serv, &gko.the_serv, g_job.uri);
            gko_quit(1);
        }
        int sleep_time = 0;
        if (gko.opt.seed_time > 0) /// seed time is set manually
        {
            sleep_time = gko.opt.seed_time;
        }
        else
        {
            if (g_job.host_set_max_size > 1)
            {
                sleep_time = g_job.dl_time.tv_sec * g_job.host_set->size()
                        / g_job.host_set_max_size / DOWN_UP_TIME_RATIO;
                if (! sleep_time)
                {
                    sleep_time++;
                }
                if (sleep_time > MAX_AUTO_SEED_TIME)
                {
                    sleep_time = MAX_AUTO_SEED_TIME;
                }
            }
            else
            {
                sleep_time = 0;
            }
        }
        gko_log(NOTICE, "upload for %d seconds", sleep_time);
        sleep(sleep_time);
    }

    quit_job_c(&gko_pool::gko_serv, &gko.the_serv, g_job.uri);
    gko_log(NOTICE, "upload end, client quited");
    fprintf(stderr, "upload end, client quited\n");
    pthread_clean();
    gko_quit(0);
}
