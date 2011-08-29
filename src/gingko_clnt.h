/*
 * gingko_clnt.h
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */

#ifndef GINGKO_CLNT_H_
#define GINGKO_CLNT_H_

/// CORE algrithm: hash the host to the data ring
s_host_hash_result_t * host_hash(s_job_t * jo, const s_host_t * new_host,
        s_host_hash_result_t * result, const u_char usage);

/// sent GET handler
GKO_INT64 get_blocks_c(s_job_t * jo, s_host_t * dhost, GKO_INT64 num,
        GKO_INT64 count,
        u_char flag, char * buf);

/// send HELO handler
int helo_serv_c(void * arg, int fd, s_host_t * server);

/// send JOIN handler
void * join_job_c(void * arg, int fd);

/// send QUIT handler
int quit_job_c(s_host_t * quit_host, s_host_t * server, char * uri);

#endif /* GINGKO_CLNT_H_ */
