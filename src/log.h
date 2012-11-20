/**
 * log.h
 *
 *  Created on: 2011-7-13
 *      Author: auxten
 **/

#ifndef LOG_H_
#define LOG_H_

///log level
static const u_char FATAL =     0;
static const u_char WARNING =   1;
static const u_char NOTICE =    2;
static const u_char TRACE =     3;
static const u_char DEBUG =     4;

/// to print the file line func
///  MUST NOT use it in "const char *fmt, ..." type func
#define FLF(a)  "{%s:%d %s} %s",__FILE__,__LINE__,__func__,#a

/// log handler
void _gko_log(const u_char log_level, const char *fmt, ...);
int lock_log(void);

/// macro using to print the file and line in log for easy debuging
/// when it is not in releasing build
#ifdef NDEBUG
#define gko_log(log_level, fmt, arg...) \
    do {\
        _gko_log(log_level, fmt, ##arg); \
    } while(0)
#else
#define gko_log(log_level, fmt, arg...) \
    do {\
        _gko_log(log_level, fmt"[%s:%d]", ##arg,  __FILE__, __LINE__);\
    } while (0)
#endif

#endif /** LOG_H_ **/
