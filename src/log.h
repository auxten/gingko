/**
 * log.h
 *
 *  Created on: 2011-7-13
 *      Author: auxten
 **/

#ifndef LOG_H_
#define LOG_H_

/// to print the file line func
///  MUST NOT use it in "const char *fmt, ..." type func
#define FLF(a)  "{%s:%d %s} %s",__FILE__,__LINE__,__func__,#a

///fatal
static const u_char FATAL =     0;
///warning
static const u_char WARNING =   1;
///notice
static const u_char NOTICE =    2;
///trace
static const u_char TRACE =     3;
///debug
static const u_char DEBUG =     4;

/// log handler
void gko_log(const u_char log_level, const char *fmt, ...);

#endif /** LOG_H_ **/
