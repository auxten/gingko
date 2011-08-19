/**
 * options.h
 *
 *  Created on: 2011-7-14
 *      Author: auxten
 **/

#ifndef OPTIONS_H_
#define OPTIONS_H_

/// process args for client
int clnt_parse_opt(int argc, char *argv[], s_job_t * jo);
/// process args for server
int serv_parse_opt(int argc, char *argv[]);

#endif /** OPTIONS_H_ **/
