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
#ifdef __APPLE__
#include <poll.h>
#else
#include <sys/poll.h>
#endif /** __APPLE__ **/

#include "config.h"
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
/////default pthread_attr_t
//extern pthread_attr_t g_attr;
///client wide lock
extern pthread_mutex_t g_clnt_lock;
///block host set lock
extern pthread_mutex_t g_blk_hostset_lock;
/////mutex for gko.hosts_new_noready
//extern pthread_mutex_t g_hosts_new_noready_mutex;
/////mutex for gko.hosts_del_noready
//extern pthread_mutex_t g_hosts_del_noready_mutex;
/************** PTHREAD STUFF **************/
//
//
GINGKO_OVERLOAD_S_HOST_LT

//
//using namespace std;
//
//
/// the g_job assoiate with the client
extern s_job_t g_job;

/// gingko global stuff
extern s_gingko_global_t gko;

/**
 * @brief CORE algrithm: hash the host to the data ring
 * @brief if result is NULL, the s_host_hash_result_t is no need
 * @brief if (usage | DEL_HOST) then del it
 * @brief else add it
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
s_host_hash_result_t * host_hash(s_job_t * jo, const s_host_t * new_host,
        s_host_hash_result_t * result, const u_char usage)
{
    set<s_host_t> * h_set;
    if (jo->block_count == 0)
    {
        return NULL;
    }
    if (result)
    {
        memset(result, 0, sizeof(s_host_hash_result_t));
    }
    GKO_UINT64 host_key = (((GKO_UINT64) inet_addr(new_host->addr)) << 32)
            + ((new_host->port) << 16);
    GKO_UINT64 vnode_distance =
            (jo->block_count / VNODE_NUM / 3) ? (host_key % (jo->block_count
                    / VNODE_NUM / 3) + (jo->block_count * 2 / VNODE_NUM / 3))
                    : 1;
    gko_log(NOTICE, "host_key: %lld, ip: %s, port: %d, usage: %d", host_key,
            new_host->addr, new_host->port, usage);
    GKO_UINT64 vnode_start = host_key % (jo->block_count);
    gko_log(NOTICE, "vnode_distance:%lld, vnode_start:%lld", vnode_distance,
            vnode_start);
    for (int i = 0; i < VNODE_NUM; i++)
    {
        GKO_INT64 vnode = (i * vnode_distance + vnode_start) % (jo->block_count);
        pthread_mutex_lock(&g_blk_hostset_lock);
        h_set = ((jo->blocks) + vnode)->host_set;
        if (!h_set)
        {
            if (usage & DEL_HOST)
            {
                gko_log(WARNING, "del host from non existed set");
                pthread_mutex_unlock(&g_blk_hostset_lock);
                continue;
            }
            else
            {
                h_set = ((jo->blocks) + vnode)->host_set = new set<s_host_t> ;
            }
        }
        if (usage & DEL_HOST)
        {
            (*h_set).erase(*new_host);
        }
        else
        {
            (*h_set).insert(*new_host);
        }

        pthread_mutex_unlock(&g_blk_hostset_lock);

        if (result)
        {
            /**
             *  check if the v_node fall in the same block
             *  this must happen when block_count < 3
             **/
            for (int j = 0; j < i; j++)
            {
                if (vnode == result->v_node[j])
                {
                    vnode = -1; /// mark the node unavailable
                    break;
                }
            }
            result->v_node[i] = vnode;
        }
        gko_log(NOTICE, "vnode: %lld", vnode);
    }
    gko_log(NOTICE, "host_key:%lld", host_key);
    return result;
}

/**
 * @brief sent GET handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_INT64 get_blocks_c(s_job_t * jo, s_host_t * dhost, GKO_INT64 num, GKO_INT64 count,
        u_char flag, char * buf)
{
    if (UNLIKELY(!count))
    {
        return 0;
    }
    int sock;
    GKO_INT64 blk_got;
    GKO_INT64 j;
    GKO_INT64 ret;
    ssize_t counter;
    char msg[MSG_LEN] = {'\0'};
    char * arg_array[4];
    GKO_INT64 n_to_recv;
    sock = connect_host(dhost, RCV_TIMEOUT, SND_TIMEOUT);
    if (FAIL_CHECK(sock == HOST_DOWN_FAIL))
    {
        /** the remote host maybe down, tell the server **/
        snprintf(msg, MSG_LEN, "DEAD\t%s\t%s\t%u", jo->uri, dhost->addr,
                dhost->port);
        gko_log(NOTICE, "telling server %s", msg);
        if (FAIL_CHECK(sendcmd(&gko.the_serv, msg, RCV_TIMEOUT, SND_TIMEOUT)))
        {
            gko_log(WARNING, "sendcmd host_down_cmd failed");
        }
    }
    if (FAIL_CHECK(sock < 0))
    {
        gko_log(WARNING, "connect_host error");
        return -1;
    }
    /**
     * send "GETT uri start num" to server
     **/
    snprintf(msg, MSG_LEN, "GETT\t%s\t%lld\t%lld", g_job.uri, num, count);
    if (FAIL_CHECK((j = sendall(sock, msg, MSG_LEN, 0)) < 0))
    {
        gko_log(NOTICE, "sent: %lld", j);
        gko_log(WARNING, "sending GETT error!");
        ret = -1;
        goto GET_BLKS_C_CLOSE_SOCK;
    }
    /**
     * server will send "HAVA n"
     **/
    readall(sock, msg, sizeof(msg), MSG_WAITALL);
    if (FAIL_CHECK(sep_arg(msg, arg_array, 2) != 2))
    {
        gko_log(WARNING, "Wrong HAVE cmd: %s", msg);
        ret = -1;
        goto GET_BLKS_C_CLOSE_SOCK;
    }
    n_to_recv = atol(arg_array[1]);
    /**
     * readall data and check the digest
     **/
    s_block_t * b;
    for (blk_got = 0; blk_got < n_to_recv; blk_got++)
    {
        b = g_job.blocks + (num + blk_got) % g_job.block_count;
        counter = readall(sock, buf, b->size, MSG_WAITALL);
        if (FAIL_CHECK(counter != b->size))
        {
            gko_log(WARNING, "%ld", counter);
            gko_log(WARNING, "not readall all");
        }
        if (LIKELY(digest_ok(buf, b)))
        {
            if (flag & W_DISK)
            {
                if (UNLIKELY(writeblock(&g_job, (u_char *) buf, b) < 0))
                {
                    ret = -1;
                    goto GET_BLKS_C_CLOSE_SOCK;
                }
                else
                {
                    b->done = 1;
                    dump_progress(&g_job, b);
                }
            }
        }
        else
        {
            gko_log(WARNING, "digest_err");
            break;
        }
        ///usleep(100);
        bw_down_limit(counter, gko.opt.limit_down_rate);
    }
    ret = blk_got;

GET_BLKS_C_CLOSE_SOCK:
    close_socket(sock);
    return ret;
}

/**
 * @brief send HELO handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int helo_serv_c(void * arg, int fd, s_host_t * server)
{
    int sock;
    int j;
    int ret;
    int select_ret;
    int res;
    socklen_t res_size = sizeof res;
    char msg[2] = {'\0'}; ///for readall HI, must not larger than 2
    struct sockaddr_in channel;
    struct sockaddr addr;///source addr
    struct timeval recv_timeout;
    struct timeval send_timeout;
    struct sockaddr_in seed_addr;///source addr_in
    recv_timeout.tv_sec = RCV_TIMEOUT;
    recv_timeout.tv_usec = 0;
    send_timeout.tv_sec = SND_TIMEOUT;
    send_timeout.tv_usec = 0;
    in_addr_t serv;
    int addr_len;
    fd_set wset;

    addr_len = getaddr_my(server->addr, &serv);
    if (!addr_len)
    {
        gko_log(FATAL, "gethostbyname error");
        return -1;
    }
    socklen_t sock_len = sizeof(addr);

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        gko_log(FATAL, "get socket error");
        return -1;
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, &serv, addr_len);
    channel.sin_port = htons(server->port);

    /** set read & write timeout time **/
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout,
                    sizeof(struct timeval))))
    {
        gko_log(WARNING, "setsockopt SO_RCVTIMEO error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &send_timeout,
                    sizeof(struct timeval))))
    {
        gko_log(WARNING, "setsockopt SO_SNDTIMEO error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        gko_log(WARNING, "set socket non-blocking error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        gko_log(WARNING, "hello connect error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    /** Wait for write bit to be set **/
    ///
#if HAVE_POLL
    {
        struct pollfd pollfd;

        pollfd.fd = sock;
        pollfd.events = POLLOUT;

        /* send_sec is in seconds, timeout in ms */
        select_ret = poll(&pollfd, 1, (int)(SND_TIMEOUT * 1000 + 1));
    }
#else
    {
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        select_ret = select(sock + 1, 0, &wset, 0, &send_timeout);
    }
#endif /* HAVE_POLL */
    if (select_ret < 0)
    {
        gko_log(FATAL, "select error on helo");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }
    else if (!select_ret)
    {
        gko_log(FATAL, "connect timeout on helo");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    (void)getsockopt(sock, SOL_SOCKET,
                     SO_ERROR, &res, &res_size);
    if(CONNECT_DEST_DOWN(res))
    {
        gko_log(WARNING, "SO_ERROR: %d", res);
        ret = HOST_DOWN_FAIL;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    /** set back blocking **/
    if (FAIL_CHECK(setblock(sock) < 0))
    {
        gko_log(WARNING, "set socket non-blocking error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }

    ///get the sockaddr and port
    /// say HELO expect HI
    if (FAIL_CHECK((j = sendall(sock, "HELO", 4, 0)) < 0))
    {
        gko_log(WARNING, "sendall HELO error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;

    }
    j = readall(sock, msg, sizeof(msg), MSG_WAITALL);
    if (-1 == getsockname(sock, &addr, &sock_len))
    {
        gko_log(WARNING, "getsockname error");
        ret = -1;
        goto HELO_SERV_C_CLOSE_SOCK;
    }
    /**
     * get the public ip addr
     **/
    memcpy(&seed_addr, &addr, sizeof(seed_addr));
    strncpy(gko.the_clnt.addr, inet_ntoa(seed_addr.sin_addr), IP_LEN);
    ///gko.the_clnt.port = ntohs(seed_addr.sin_port);
    gko_log(NOTICE, "source: %s:%d", inet_ntoa(seed_addr.sin_addr),
            ntohs(seed_addr.sin_port));
    ret = 0;

HELO_SERV_C_CLOSE_SOCK:
    close_socket(sock);
    return ret;
}

/**
 * @brief send the NEWW to all related clients
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static int broadcast_join(s_host_t * host_array, s_host_t *h)
{
    s_host_t * p_h = host_array;
    char buf[SHORT_MSG_LEN] =
        { '\0' };
    snprintf(buf, SHORT_MSG_LEN, "NEWW\t%s\t%d", h->addr, h->port);
    while (p_h->port)
    {
        sendcmd(p_h, buf, 2, 2);
        p_h++;
    }
    return 0;
}

/**
 * @brief send JOIN handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * join_job_c(void * arg, int fd)
{
    char msg[MSG_LEN] = {'\0'};
    int sock;
    int j;
    void * ret;
    char * client_local_path = (char *) calloc(1, MAX_PATH_LEN);
    s_host_t * host_buf;
    char broadcast_buf[SHORT_MSG_LEN] =
        { '\0' };
    snprintf(msg, MAX_URI, "JOIN\t%s\t%s\t%d", g_job.uri, gko.the_clnt.addr,
            gko.the_clnt.port);
    sock = connect_host(&gko.the_serv, 10, SND_TIMEOUT);
    if (sock < 0)
    {
        gko_log(WARNING, "connect_host error");
        free(client_local_path);
        client_local_path = NULL;
        return (void *) -1;
    }

    if ((j = sendall(sock, msg, MSG_LEN, 0)) < 0)
    {
        gko_log(NOTICE, "sent: %d", j);
        gko_log(WARNING, "sending JOIN error!");
        ret = (void *) -1;
        goto JOIN_JOB_C_CLOSE_SOCK;
    }
    /**
     * readall the hash progress info until it's 100
     **/
    GKO_INT64 percent;
    do
    {
        if (readall(sock, &percent, sizeof(percent), MSG_WAITALL) < 0)
        {
            gko_log(WARNING, "readall progress error!");
            ret = (void *) -1;
            goto JOIN_JOB_C_CLOSE_SOCK;
        }
        gko_log(NOTICE, "make seed progress: %lld\%", percent);
    } while (percent != 100);

    /**
     * store the local path, recover it after readall the g_job struct
     **/
    strncpy(client_local_path, g_job.path, MAX_PATH_LEN);

    /**
     *  read and init some struct from serv
     **/
    j = readall(sock, (void *) (&g_job), sizeof(g_job), MSG_WAITALL);
    gko_log(NOTICE, "g_job have read %d of %ld", j, sizeof(g_job));
    strncpy(g_job.path, client_local_path, MAX_PATH_LEN);
    gko_log(NOTICE, "g_job.file_count %lld", g_job.file_count);
    switch (g_job.job_state)
    {
        case JOB_FILE_TYPE_ERR:
            gko_log(FATAL, "src file type error!");
            ret = (void *) -1;
            goto JOIN_JOB_C_CLOSE_SOCK;
            break;

        case JOB_FILE_OPEN_ERR:
            gko_log(FATAL, "src file open error!");
            ret = (void *) -1;
            goto JOIN_JOB_C_CLOSE_SOCK;
            break;

        case JOB_RECURSE_ERR:
            gko_log(FATAL, "src file recurse error!");
            ret = (void *) -1;
            goto JOIN_JOB_C_CLOSE_SOCK;
            break;

        default:
            break;
    }

    if (g_job.file_count)
    {
        ///read s_file_t
        g_job.files = (s_file_t *) calloc(1, g_job.files_size);
        if (!g_job.files)
        {
            gko_log(FATAL, "g_job.files calloc failed");
            ret = (void *) -2;
            goto JOIN_JOB_C_CLOSE_SOCK;
        }
        j = readall(sock, g_job.files, g_job.files_size, MSG_WAITALL);
        gko_log(NOTICE, "files seed have read %d of %lld", j, g_job.files_size);
    }
    else
    {
        ///no file to process just quit
        ret = (void *) -1;
        goto JOIN_JOB_C_CLOSE_SOCK;
    }

    if (g_job.block_count)
    {
        ///read s_block_t
        g_job.blocks = (s_block_t *) calloc(1, g_job.blocks_size);
        if (!g_job.blocks)
        {
            gko_log(FATAL, "g_job.blocks calloc failed");
            ret = (void *) -2;
            goto JOIN_JOB_C_CLOSE_SOCK;
        }
        j = readall(sock, g_job.blocks, g_job.blocks_size, MSG_WAITALL);
        gko_log(NOTICE, "blocks seed have read %d of %lld", j, g_job.blocks_size);
    }

    ///read hosts
    host_buf = (s_host_t *) calloc(g_job.host_num + 1, sizeof(s_host_t));
    if (!host_buf)
    {
        gko_log(FATAL, "s_host_t buf calloc failed");
        ret = (void *) -2;
        goto JOIN_JOB_C_CLOSE_SOCK;
    }
    j = readall(sock, host_buf, g_job.host_num * sizeof(s_host_t), MSG_WAITALL);
    gko_log(NOTICE, "hosts have read %d of %ld", j, g_job.host_num * sizeof(s_host_t));

    ///put hosts into g_job.host_set
    g_job.host_set = new set<s_host_t> ;
    pthread_mutex_lock(&g_clnt_lock);
    (*(g_job.host_set)).insert(host_buf, host_buf + g_job.host_num);
    pthread_mutex_unlock(&g_clnt_lock);

    free(host_buf);
    host_buf = NULL;

    snprintf(broadcast_buf, SHORT_MSG_LEN, "NEWW\t%s\t%d", gko.the_clnt.addr, gko.the_clnt.port);
    for (set<s_host_t>::iterator i = (*(g_job.host_set)).begin(); i
            != (*(g_job.host_set)).end(); i++)
    {
        sendcmd((s_host_t *)&(*i), broadcast_buf, 2, 2);
    }
    /// put the known host in the hash ring
    pthread_mutex_lock(&g_clnt_lock);
    for (set<s_host_t>::iterator i = (*(g_job.host_set)).begin(); i
            != (*(g_job.host_set)).end(); i++)
    {
        gko_log(NOTICE, "host in set:%s %d", i->addr, i->port);
        host_hash(&g_job, &(*i), NULL, ADD_HOST);
    }
    pthread_mutex_unlock(&g_clnt_lock);
    ///	if (g_job.file_count) {
    ///		printf("host_num:%d, uri:%s, file1:%s, HOST:%s", g_job.host_num,
    ///				g_job.uri, g_job.files->name, ((*(g_job.host_set)).begin())->addr);
    ///	}
    gko_log(NOTICE, "g_job.block_count: %lld", g_job.block_count);
    if (g_job.block_count)
    {
        pthread_mutex_lock(&g_blk_hostset_lock);
        for (j = 0; j < g_job.block_count; j++)
        {
            if ((g_job.blocks + j)->host_set)
            {
                for (set<s_host_t>::iterator it =
                    (*((g_job.blocks + j)->host_set)).begin(); it
                        != (*((g_job.blocks + j)->host_set)).end(); it++)
                {
                    gko_log(NOTICE, "%d %s:%d", j, it->addr, it->port);
                }
            }
        }
        pthread_mutex_unlock(&g_blk_hostset_lock);
    }
    ret = (void *) 0;

JOIN_JOB_C_CLOSE_SOCK:
    free(client_local_path);
    client_local_path = NULL;
    close_socket(sock);
    return ret;
}

/**
 * @brief send QUIT handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int quit_job_c(s_host_t * quit_host, s_host_t * server, char * uri)
{
    extern struct event_base *g_ev_base;
    char msg[MSG_LEN] = {'\0'};
    /**
     * send "QUIT uri host port" to server
     **/
    snprintf(msg, MSG_LEN, "QUIT\t%s\t%s\t%d", g_job.uri, quit_host->addr,
            quit_host->port);
    gko_log(NOTICE, "quiting : '%s'", msg);
    if(sendcmd(server, msg, 2, 2) < 0)
    {
        gko_log(NOTICE, "sending quit message failed");
    }

    if(event_base_loopbreak(g_ev_base) != 0)
    {
        gko_log(FATAL, "event_base_loopbreak failed");
    }
    else
    {
        gko_log(NOTICE, "event_base_loopbreak succeed");
    }

    for (GKO_INT64 i = 0; i < g_job.block_count; i++)
    {
        delete (g_job.blocks + i)->host_set;
    }
    if (g_job.blocks)
    {
        free(g_job.blocks);
        g_job.blocks = NULL;
    }
    if (g_job.files)
    {
        free(g_job.files);
        g_job.files = NULL;
    }
    if (g_job.host_set)
    {
        delete g_job.host_set;
        g_job.host_set = NULL;
    }

    ///printf("sent: %s", msg);
    return 0;
}

/**
 * @brief download the vnode area
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
