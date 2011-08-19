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

#include "gingko.h"
#include "log.h"

extern s_gingko_global_t gko;

/**
 * @brief loglevel
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static const char * LOG_DIC[] =
    { "FATAL:   ", "WARNING: ", "NOTICE:  ", "TRACE:   ", "DEBUG:   ", };

/**
 * @brief generate the time string according to the TIME_FORMAT
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char * gettimestr(char * time)
{
    struct timeval tv;
    struct tm ltime;
    time_t curtime;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    ///Format time
    strftime(time, 25, TIME_FORMAT, localtime_r(&curtime, &ltime));
    return time;
}

/**
 * @brief log handler
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void gko_log(const u_char log_level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char logstr[256];

    if(UNLIKELY(! gko.log_fp))
    {
        gko.log_fp = fopen(gko.opt.logpath, "a+");
        if(! gko.log_fp)
        {
            fprintf(stderr, "cann't open log file %s, see help", gko.opt.logpath);
            exit(1);
        }
    }
    snprintf(logstr, sizeof(logstr), "%s", LOG_DIC[log_level]);
    gettimestr(logstr + strlen(logstr));
    vsnprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr), fmt,
            args);
    if (log_level < 2)
    {
        snprintf(logstr + strlen(logstr), sizeof(logstr) - strlen(logstr),
                "; #");
        strerror_r(errno, logstr + strlen(logstr),
                sizeof(logstr) - strlen(logstr));
    }
    fprintf(gko.log_fp, "%s\n", logstr);
    fflush(gko.log_fp);
    va_end(args);
    return;
}
