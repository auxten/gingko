/*
 * progress.cpp
 *
 *  Created on: 2011-9-12
 *      Author: auxten
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "gingko.h"


///mutex for up bandwidth limit
static pthread_mutex_t progress_lock = PTHREAD_MUTEX_INITIALIZER;

extern s_job_t g_job;

int show_progress(int count)
{
    static GKO_INT64 done_count = 0;
//    static struct timeval last_tval = {0,0};
//    struct timeval this_tval;
//    struct timeval diff_tval;
//    GKO_INT64 diff_usec;

    if (count > 0)
    {
//        gettimeofday(&this_tval, NULL);
        pthread_mutex_lock(&progress_lock);
//        timersub(&this_tval, &last_tval, &diff_tval);
//        last_tval.tv_sec = this_tval.tv_sec;
//        last_tval.tv_usec = this_tval.tv_usec;
//        diff_usec = diff_tval.tv_sec * 1000000 + diff_tval.tv_usec;

        done_count += count;
        pthread_mutex_unlock(&progress_lock);
    }
//    if (g_job.block_count && diff_usec)
    if (g_job.block_count)
    {
//        printf("\rDownload progress:\t%5.2f %% ,speed:\t%7.2f MB/s\r", (double)done_count * 100 / g_job.block_count, count * BLOCK_SIZE / (double)1.048576 /diff_usec);
        printf("\rDownload progress:\t%5.2f %%\r", (double)done_count * 100 / g_job.block_count);
    }
    fflush(stdout);
    return 0;
}


