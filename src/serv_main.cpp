/*
 * serv_main.cpp
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */


#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /** GINGKO_SERV **/

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
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /** __APPLE__ **/

#include <map>
#include <string>
#include <list>
#include <algorithm>

#include "gingko.h"
#include "async_pool.h"
#include "hash/xor_hash.h"
#include "path.h"
#include "log.h"
#include "seed.h"
#include "socket.h"
#include "option.h"
#include "job_state.h"
#include "gingko_serv.h"

using namespace std;

/************** PTHREAD STUFF **************/
///server wide lock
pthread_mutex_t g_grand_lock;
///job specific lock
s_lock_t g_job_lock[MAX_JOBS];
pthread_key_t g_dir_key;
/************** PTHREAD STUFF **************/

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT

/// jobs map
map<string, s_job_t *> g_m_jobs;

/// gingko global stuff
s_gingko_global_t gko;


/**
 * @brief daemonlize
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int init_daemon(void)
{
    int fd;
    switch (fork())
    {
        case -1:
            return (-1);
            break;

        case 0:
            break;

        default:
            _exit(EXIT_SUCCESS);
            break;
    }

    if (setsid() == -1)
    {
        return (-1);
    }

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1)
    {
        if (dup2(fd, STDIN_FILENO) < 0)
        {
            gko_log(FATAL, "dup2 stdin");
            return (-1);
        }
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            gko_log(FATAL, "dup2 stdout");
            return (-1);
        }
        if (dup2(fd, STDERR_FILENO) < 0)
        {
            gko_log(FATAL, "dup2 stderr");
            return (-1);
        }

        if (fd > STDERR_FILENO)
        {
            if (close(fd) < 0)
            {
                gko_log(FATAL, "close /dev/null failed");
                return (-1);
            }
        }
    }

    return 0;
}

/**
 * @brief pthread stuff init
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int pthread_init()
{
    if (pthread_key_create(&g_dir_key, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_key_create error"));
        return -1;
    }
    if (pthread_mutex_init(&g_grand_lock, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_init error"));
        return -1;
    }
    for (int i = 0; i < MAX_JOBS; i++)
    {
        g_job_lock[i].state = LK_FREE;
        if (pthread_mutex_init(&(g_job_lock[i].lock), NULL) != 0)
        {
            gko_log(FATAL, FLF("pthread_mutex_init error"));
            return -1;
        }
    }
    return 0;
}

/**
 * @brief pthread stuff clean
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int pthread_clean()
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (pthread_mutex_destroy(&(g_job_lock[i].lock)) != 0)
        {
            gko_log(FATAL, FLF("pthread_mutex_init error"));
            return -1;
        }
    }
    if (pthread_mutex_destroy(&g_grand_lock) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_init error"));
        return -1;
    }
    if (pthread_key_delete(g_dir_key) != 0)
    {
        gko_log(FATAL, FLF("pthread_mutex_init error"));
        return -1;
    }

    return 0;
}

/**
 * @brief server INT handeler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void serv_int_handler(const int sig)
{
    gko.sig_flag = sig;
    return;
}

/**
 * @brief this thread will watch the gko.sig_flag and take action
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-9
 **/
GKO_STATIC_FUNC void * serv_int_worker(void * a)
{
    while(1)
    {
        if (UNLIKELY(gko.sig_flag))
        {
            gko_log(WARNING, "SIGNAL handled, server terminated");
            /// Clear all status
            conn_close();
            exit(2);
        }
        usleep(CK_SIG_INTERVAL);
    }
}

/**
 * @brief server global stuff init also the args processing
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int gingko_serv_global_init(int argc, char *argv[])
{
    memset(&gko, 0, sizeof(gko));
    if(serv_parse_opt(argc, argv) == 0)
    {
        gko_log(NOTICE, "opts parsed successfully");
    }
    else
    {
        return -1;
    }
    umask(0);

    if(check_ulimit() != 0)
    {
        return -1;
    }

    if (init_daemon())
    {
        gko_log(FATAL, "init_daemon failed");
        return -1;
    }
    set_sig(serv_int_handler);
    /**
     * init global vars stuff
     **/
    gko.ready_to_serv = 1;
    gko.cmd_list_p = g_cmd_list;
    gko.func_list_p = g_func_list_s;
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
    ///init_daemon();
    if(gingko_serv_global_init(argc, argv))
    {
        gko_log(FATAL, "gingko_serv_global_init failed");
        fprintf(stderr, "Server error, quited\n");
        exit(1);
    }

    gko_log(DEBUG, "Debug mode start, i will print tons of log :p!");

    if (pthread_init() != 0)
    {
        gko_log(FATAL, FLF("pthread_init error"));
        fprintf(stderr, "Server error, quited\n");
        exit(1);
    }
    if (sig_watcher(serv_int_worker))
    {
        gko_log(FATAL, "signal watcher start error");
        fprintf(stderr, "Server error, quited\n");
        exit(1);
    }
    if (gingko_serv_async_server() != 0)
    {
        gko_log(WARNING, "gingko_serv_async_server error");
        exit(1);
    }
    pthread_clean();
    return 0;
}


