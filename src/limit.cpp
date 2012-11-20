/**
 * limit.cpp
 *
 *  Created on: 2011-8-4
 *      Author: auxten
 **/
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include "gingko.h"


///mutex for up bandwidth limit
static pthread_mutex_t g_bw_up_mutex = PTHREAD_MUTEX_INITIALIZER;
///mutex for down bandwidth limit
static pthread_mutex_t g_bw_down_mutex = PTHREAD_MUTEX_INITIALIZER;
///mutex for up bandwidth limit
static pthread_mutex_t g_disk_r_mutex = PTHREAD_MUTEX_INITIALIZER;
///mutex for down bandwidth limit
static pthread_mutex_t g_disk_w_mutex = PTHREAD_MUTEX_INITIALIZER;
///mutex for make seed limit
static pthread_mutex_t g_mk_seed_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief limit down rate
 *
 * @see
 * @note bwlimit modified from scp
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void bw_down_limit(int amount, int limit_rate)
{
    static struct timeval bw_down_start;
    static struct timeval bw_down_end;
    static int down_lamt;
    static int down_thresh = 16384;
    GKO_UINT64 waitlen;
    struct timespec ts;
    struct timespec rm;

    if (amount <= 0 || limit_rate <= 0)
    {
        return;
    }
    pthread_mutex_lock(&g_bw_down_mutex);

    if (UNLIKELY(!timerisset(&bw_down_start)))
    {
        gettimeofday(&bw_down_start, NULL);
        goto DISK_W_UNLOCK_RET;
    }

    down_lamt += amount;
    if (down_lamt < down_thresh)
    {
        goto DISK_W_UNLOCK_RET;
    }

    gettimeofday(&bw_down_end, NULL);
    timersub(&bw_down_end, &bw_down_start, &bw_down_end);
    if (!timerisset(&bw_down_end))
    {
        goto DISK_W_UNLOCK_RET;
    }

    waitlen = (GKO_UINT64) 1000000L * down_lamt / limit_rate;

    bw_down_start.tv_sec = waitlen / 1000000L;
    bw_down_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&bw_down_start, &bw_down_end, >))
    {
        timersub(&bw_down_start, &bw_down_end, &bw_down_end);

        /** Adjust the wait time **/
        if (bw_down_end.tv_sec)
        {
            down_thresh /= 2;
            if (down_thresh < 2048)
            {
                down_thresh = 2048;
            }
        }
        else if (bw_down_end.tv_usec < 100)
        {
            down_thresh *= 2;
            if (down_thresh > 32768)
            {
                down_thresh = 32768;
            }
        }

        TIMEVAL_TO_TIMESPEC(&bw_down_end, &ts);
        ///gko_log(WARNING, "sleep for: %d usec", (&bw_down_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1)
        {
            if (errno != EINTR)
            {
                break;
            }
            ts = rm;
        }
    }

    down_lamt = 0;
    gettimeofday(&bw_down_start, NULL);

DISK_W_UNLOCK_RET:
    pthread_mutex_unlock(&g_bw_down_mutex);
    return;
}

/**
 * @brief limit up rate
 *
 * @see
 * @note bwlimit modified from scp
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void bw_up_limit(int amount, int limit_rate)
{
    static struct timeval bw_up_start;
    static struct timeval bw_up_end;
    static int up_lamt;
    static int up_thresh = 16384;
    GKO_UINT64 waitlen;
    struct timespec ts;
    struct timespec rm;

    if (amount <= 0 || limit_rate <= 0)
    {
        return;
    }
    pthread_mutex_lock(&g_bw_up_mutex);

    if (UNLIKELY(!timerisset(&bw_up_start)))
    {
        gettimeofday(&bw_up_start, NULL);
        goto UP_UNLOCK_RET;
    }

    up_lamt += amount;
    if (up_lamt < up_thresh)
    {
        goto UP_UNLOCK_RET;
    }

    gettimeofday(&bw_up_end, NULL);
    timersub(&bw_up_end, &bw_up_start, &bw_up_end);
    if (!timerisset(&bw_up_end))
    {
        goto UP_UNLOCK_RET;
    }

    waitlen = (GKO_UINT64) 1000000L * up_lamt / limit_rate;

    bw_up_start.tv_sec = waitlen / 1000000L;
    bw_up_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&bw_up_start, &bw_up_end, >))
    {
        timersub(&bw_up_start, &bw_up_end, &bw_up_end);

        /** Adjust the wait time **/
        if (bw_up_end.tv_sec)
        {
            up_thresh /= 2;
            if (up_thresh < 2048)
            {
                up_thresh = 2048;
            }
        }
        else if (bw_up_end.tv_usec < 100)
        {
            up_thresh *= 2;
            if (up_thresh > 32768)
            {
                up_thresh = 32768;
            }
        }

        TIMEVAL_TO_TIMESPEC(&bw_up_end, &ts);
        ///gko_log(WARNING, "sleep for: %d usec", (&bw_up_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1)
        {
            if (errno != EINTR)
            {
                break;
            }
            ts = rm;
        }
    }

    up_lamt = 0;
    gettimeofday(&bw_up_start, NULL);

UP_UNLOCK_RET:
    pthread_mutex_unlock(&g_bw_up_mutex);
    return;
}

/**
 * @brief limit disk write rate
 *
 * @see
 * @note bwlimit modified from scp
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void disk_w_limit(int amount, int limit_rate)
{
    static struct timeval disk_w_start;
    static struct timeval disk_w_end;
    static int write_lamt;
    static int write_thresh = 16384;
    GKO_UINT64 waitlen;
    struct timespec ts;
    struct timespec rm;

    if (amount <= 0 || limit_rate <= 0)
    {
        return;
    }
    pthread_mutex_lock(&g_disk_w_mutex);

    if (UNLIKELY(!timerisset(&disk_w_start)))
    {
        gettimeofday(&disk_w_start, NULL);
        goto DISK_W_UNLOCK_RET;
    }

    write_lamt += amount;
    if (write_lamt < write_thresh)
    {
        goto DISK_W_UNLOCK_RET;
    }

    gettimeofday(&disk_w_end, NULL);
    timersub(&disk_w_end, &disk_w_start, &disk_w_end);
    if (!timerisset(&disk_w_end))
    {
        goto DISK_W_UNLOCK_RET;
    }

    waitlen = (GKO_UINT64) 1000000L * write_lamt / limit_rate;

    disk_w_start.tv_sec = waitlen / 1000000L;
    disk_w_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&disk_w_start, &disk_w_end, >))
    {
        timersub(&disk_w_start, &disk_w_end, &disk_w_end);

        /** Adjust the wait time **/
        if (disk_w_end.tv_sec)
        {
            write_thresh /= 2;
            if (write_thresh < 2048)
            {
                write_thresh = 2048;
            }
        }
        else if (disk_w_end.tv_usec < 100)
        {
            write_thresh *= 2;
            if (write_thresh > 32768)
            {
                write_thresh = 32768;
            }
        }

        TIMEVAL_TO_TIMESPEC(&disk_w_end, &ts);
        ///gko_log(WARNING, "sleep for: %d usec", (&disk_w_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1)
        {
            if (errno != EINTR)
            {
                break;
            }
            ts = rm;
        }
    }

    write_lamt = 0;
    gettimeofday(&disk_w_start, NULL);

DISK_W_UNLOCK_RET:
    pthread_mutex_unlock(&g_disk_w_mutex);
    return;
}

/**
 * @brief limit disk read rate
 *
 * @see
 * @note bwlimit modified from scp
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void disk_r_limit(int amount, int limit_rate)
{
    static struct timeval disk_r_start;
    static struct timeval disk_r_end;
    static int read_lamt;
    static int read_thresh = 16384;
    GKO_UINT64 waitlen;
    struct timespec ts;
    struct timespec rm;

    if (amount <= 0 || limit_rate <= 0)
    {
        return;
    }
    pthread_mutex_lock(&g_disk_r_mutex);

    if (UNLIKELY(!timerisset(&disk_r_start)))
    {
        gettimeofday(&disk_r_start, NULL);
        goto DISK_R_UNLOCK_RET;
    }

    read_lamt += amount;
    if (read_lamt < read_thresh)
    {
        goto DISK_R_UNLOCK_RET;
    }

    gettimeofday(&disk_r_end, NULL);
    timersub(&disk_r_end, &disk_r_start, &disk_r_end);
    if (!timerisset(&disk_r_end))
    {
        goto DISK_R_UNLOCK_RET;
    }

    waitlen = (GKO_UINT64) 1000000L * read_lamt / limit_rate;

    disk_r_start.tv_sec = waitlen / 1000000L;
    disk_r_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&disk_r_start, &disk_r_end, >))
    {
        timersub(&disk_r_start, &disk_r_end, &disk_r_end);

        /** Adjust the wait time **/
        if (disk_r_end.tv_sec)
        {
            read_thresh /= 2;
            if (read_thresh < 2048)
            {
                read_thresh = 2048;
            }
        }
        else if (disk_r_end.tv_usec < 100)
        {
            read_thresh *= 2;
            if (read_thresh > 32768)
            {
                read_thresh = 32768;
            }
        }

        TIMEVAL_TO_TIMESPEC(&disk_r_end, &ts);
        ///gko_log(WARNING, "sleep for: %d usec", (&disk_r_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1)
        {
            if (errno != EINTR)
            {
                break;
            }
            ts = rm;
        }
    }

    read_lamt = 0;
    gettimeofday(&disk_r_start, NULL);

DISK_R_UNLOCK_RET:
    pthread_mutex_unlock(&g_disk_r_mutex);
    return;
}


/**
 * @brief limit make seed rate
 *
 * @see
 * @note bwlimit modified from scp
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void mk_seed_limit(int amount, int limit_rate)
{
    static struct timeval mk_seed_start;
    static struct timeval mk_seed_end;
    static int mk_seed_lamt;
    static int mk_seed_thresh = 16384;
    GKO_UINT64 waitlen;
    struct timespec ts;
    struct timespec rm;
    if (amount <= 0 || limit_rate <= 0)
    {
        return;
    }
    pthread_mutex_lock(&g_mk_seed_mutex);

    if (UNLIKELY(!timerisset(&mk_seed_start)))
    {
        gettimeofday(&mk_seed_start, NULL);
        goto MK_SEED_UNLOCK_RET;
    }

    mk_seed_lamt += amount;
    if (mk_seed_lamt < mk_seed_thresh)
    {
        goto MK_SEED_UNLOCK_RET;
    }

    gettimeofday(&mk_seed_end, NULL);
    timersub(&mk_seed_end, &mk_seed_start, &mk_seed_end);
    if (!timerisset(&mk_seed_end))
    {
        goto MK_SEED_UNLOCK_RET;
    }

    waitlen = (GKO_UINT64) 1000000L * mk_seed_lamt / limit_rate;

    mk_seed_start.tv_sec = waitlen / 1000000L;
    mk_seed_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&mk_seed_start, &mk_seed_end, >))
    {
        timersub(&mk_seed_start, &mk_seed_end, &mk_seed_end);

        /** Adjust the wait time **/
        if (mk_seed_end.tv_sec)
        {
            mk_seed_thresh /= 2;
            if (mk_seed_thresh < 2048)
            {
                mk_seed_thresh = 2048;
            }
        }
        else if (mk_seed_end.tv_usec < 100)
        {
            mk_seed_thresh *= 2;
            if (mk_seed_thresh > 32768)
            {
                mk_seed_thresh = 32768;
            }
        }

        TIMEVAL_TO_TIMESPEC(&mk_seed_end, &ts);
        ///gko_log(WARNING, "sleep for: %d usec", (&mk_seed_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1)
        {
            if (errno != EINTR)
            {
                break;
            }
            ts = rm;
        }
    }

    mk_seed_lamt = 0;
    gettimeofday(&mk_seed_start, NULL);

MK_SEED_UNLOCK_RET:
    pthread_mutex_unlock(&g_mk_seed_mutex);
    return;
}

