/**
 * log.cpp
 *
 *  Created on: 2011-7-13
 *      Author: auxten
 **/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "gingko.h"
#include "log.h"

extern s_gingko_global_t gko;
pthread_mutex_t logcut_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief loglevel
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static const char * LOG_DIC[] =
    { "FATAL:   ", "WARNING: ", "NOTICE:  ", "TRACE:   ", "DEBUG:   ", };

/**
 * @brief generate the time string according to the TIME_FORMAT
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char * gettimestr(char * time, const char * format)
{
    struct timeval tv;
    struct tm ltime;
    time_t curtime;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    ///Format time
    strftime(time, 25, format, localtime_r(&curtime, &ltime));
    return time;
}

/**
 * @brief log handler
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void _gko_log(const u_char log_level, const char *fmt, ...)
{
    if (gko.opt.to_debug || log_level < DEBUG )
    {
        int errnum = errno;
        va_list args;
        va_start(args, fmt);
        char logstr[256] = {'\0'};
        char oldlogpath[MAX_PATH_LEN] = {'\0'};
        static FILE * lastfp = NULL;
        static GKO_INT64 counter = 1;

        snprintf(logstr, sizeof(logstr), "%s[%u]", LOG_DIC[log_level], gko_gettid());
        gettimestr(logstr + strlen(logstr), TIME_FORMAT);
        vsnprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr), fmt,
                args);
        if (log_level < NOTICE)
        {
            snprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr),
                    "; ");
            strerror_r(errnum, logstr + strlen(logstr),
                    sizeof(logstr) - strlen(logstr));
        }

        pthread_mutex_lock(&logcut_lock);
        if (gko.opt.logpath[0]  == '\0')
        {
            gko.log_fp = stdout;
        }
        else
        {
            counter ++;
            if (counter % MAX_LOG_REOPEN_LINE == 0)
            {
                if (lastfp)
                {
                    fclose(lastfp);
                }
                lastfp = gko.log_fp;
                if (counter % MAX_LOG_LINE == 0)
                {
                    strncpy(oldlogpath, gko.opt.logpath, MAX_PATH_LEN);
                    gettimestr(oldlogpath + strlen(oldlogpath), OLD_LOG_TIME);
                    rename(gko.opt.logpath, oldlogpath);
                }
                gko.log_fp = fopen(gko.opt.logpath, "a+");
            }
            if(UNLIKELY(! gko.log_fp))
            {
                gko.log_fp = fopen(gko.opt.logpath, "a+");
                if(! gko.log_fp)
                {
                    perror("Cann't open log file");
                    exit(1);
                }
            }
        }
        fprintf(gko.log_fp, "%s\n", logstr);
        fflush(gko.log_fp);
        pthread_mutex_unlock(&logcut_lock);

        va_end(args);
    }
    return;
}

int lock_log(void)
{
    return pthread_mutex_lock(&logcut_lock);
}
