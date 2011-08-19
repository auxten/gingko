/**
 *  gingko_serv.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/


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

using namespace std;

/************** PTHREAD STUFF **************/
///server wide lock
extern pthread_rwlock_t g_grand_lock;
///job specific lock
extern s_lock_t g_job_lock[MAX_JOBS];
/************** PTHREAD STUFF **************/

GINGKO_OVERLOAD_S_HOST_LT


/**
 * @brief erase job related stuff only for server
 *
 * @see s_job_t struct
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int erase_job(string &uri_string)
{
    int ret;
    s_job_t *jo;
    /// jobs map
    map<string, s_job_t *> g_m_jobs;
    map<string, s_job_t *>::iterator it;

    {
        pthread_rwlock_rdlock(&g_grand_lock);
        if ((it = g_m_jobs.find(uri_string)) == g_m_jobs.end())
        {
            ret = -1;
            pthread_rwlock_unlock(&g_grand_lock);
            goto ERASE_RET;
        }
        jo = it->second;
        /** then erase job for the job map **/
        g_m_jobs.erase(uri_string);
        gko_log(NOTICE, "g_m_jobs.size: %lld", (GKO_UINT64) g_m_jobs.size());
        pthread_rwlock_unlock(&g_grand_lock);
    }
    /** cancel the hash threads if any **/
    for (int i = 0; i < XOR_HASH_TNUM; i++)
    {
        if (! pthread_cancel(jo->hash_worker[i]))
        {
            gko_log(NOTICE, "hash thread %d canceling", i);
        }
        else
        {
            gko_log(FATAL, "hash thread %d cancel failed", i);
        }
    }

    /**
     * sleep for about write timeout time + 2
     * we must wait for the send thread call write
     * timeout and stop sending blocks. then we
     * can erase the job successfully
     **/
    sleep(SND_TIMEOUT + 2);


    /** clean the job struct **/
    pthread_rwlock_wrlock(&g_job_lock[jo->lock_id].lock);
    if (jo->blocks)
    {
        free(jo->blocks);
        jo->blocks = NULL;
    }
    if (jo->files)
    {
        free(jo->files);
        jo->files = NULL;
    }
    if (jo->host_set)
    {
        delete jo->host_set;
        jo->host_set = NULL;
    }
    for (int i = 0; i < XOR_HASH_TNUM; i++)
    {
        if (jo->hash_buf[i])
        {
            free(jo->hash_buf[i]);
            jo->hash_buf[i] = NULL;
        }
    }
    pthread_rwlock_unlock(&g_job_lock[jo->lock_id].lock);

    /** for safety re-init the rwlock **/
    pthread_rwlock_destroy(&(g_job_lock[jo->lock_id].lock));
    pthread_rwlock_init(&(g_job_lock[jo->lock_id].lock), NULL);
    g_job_lock[jo->lock_id].state = LK_FREE;

    free(jo);
    ret = 0;

ERASE_RET:
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
 int broadcast_join(s_host_t * host_array, s_host_t *h)
{
    /// jobs map
    map<string, s_job_t *> g_m_jobs;
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
