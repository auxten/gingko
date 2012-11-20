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

    if (count > 0)
    {
        pthread_mutex_lock(&progress_lock);
        done_count += count;
        pthread_mutex_unlock(&progress_lock);
    }

    if (g_job.block_count)
    {
        printf("\rDownload progress:\t%5.2f %%", (double)done_count * 100 / g_job.block_count);
    }
    fflush(stdout);
    return 0;
}


