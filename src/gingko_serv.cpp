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


/************** PTHREAD STUFF **************/
///server wide lock
extern pthread_mutex_t g_grand_lock;
///job specific lock
extern s_lock_t g_job_lock[MAX_JOBS];
/************** PTHREAD STUFF **************/

GINGKO_OVERLOAD_S_HOST_LT
extern std::map<std::string, s_job_t *> g_m_jobs;


/**
 * @brief erase job related stuff only for server
 *
 * @see s_job_t struct
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int erase_job(std::string &uri_string)
{
    int ret;
    s_job_t *jo;
    GKO_INT64 progress;
    GKO_INT64 percent;

    /// jobs map
    std::map<std::string, s_job_t *>::iterator it;

    {
        pthread_mutex_lock(&g_grand_lock);
        if ((it = g_m_jobs.find(uri_string)) == g_m_jobs.end())
        {
            ret = -1;
            gko_log(FATAL, "erase %s fail", uri_string.c_str());
            pthread_mutex_unlock(&g_grand_lock);
            goto ERASE_RET;
        }
        jo = it->second;
        jo->job_state = JOB_TO_BE_ERASED;
        /** then erase job for the job map **/
        g_m_jobs.erase(uri_string);
        gko_log(NOTICE, "g_m_jobs.size: %lld", (GKO_UINT64) g_m_jobs.size());
        pthread_mutex_unlock(&g_grand_lock);
    }
    /** cancel the hash threads if any progress < 99% **/
    progress = array_sum(jo->hash_progress, XOR_HASH_TNUM);
    percent = jo->total_size ? progress * 100 / jo->total_size : 100;

    if (percent < 100)
    {
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
    }

    /**
     * sleep for about write timeout time + 2
     * we must wait for the send thread call write
     * timeout and stop sending blocks. then we
     * can erase the job successfully
     **/
    sleep(ERASE_JOB_MEM_WAIT);


    /** clean the job struct **/
    pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
    if (jo->blocks && jo->block_count)
    {
        delete [](jo->blocks);
        jo->blocks = NULL;
    }
    if (jo->files && jo->file_count)
    {
        delete [](jo->files);
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
            delete [](jo->hash_buf[i]);
            jo->hash_buf[i] = NULL;
        }
    }
    pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);

    /** for safety re-init the rwlock **/
    pthread_mutex_destroy(&(g_job_lock[jo->lock_id].lock));
    pthread_mutex_init(&(g_job_lock[jo->lock_id].lock), NULL);
    g_job_lock[jo->lock_id].state = LK_FREE;

    delete jo;
    gko_log(NOTICE, "job '%s' erased", uri_string.c_str());
    ret = 0;

ERASE_RET:
    return ret;
}

