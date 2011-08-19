/*
 * unittest.h
 *
 *  Created on: 2011-8-16
 *      Author: auxten
 */

#ifdef UNITTEST
///async_conn.cpp (3 matches)
int conn_client_list_init();
int gingko_serv_async_server_base_init();
int gingko_clnt_async_server_base_init(s_host_t * the_host);

///async_threads.cpp (2 matches)
void thread_worker_new(int id);
int thread_list_find_next();

///clnt_main.cpp (8 matches)
void * vnode_download(void * arg);
int node_download(void *);
void * downloadworker(void *);
inline void pthread_init();
inline void pthread_clean();
void clnt_int_handler(const int sig);
void * clnt_int_worker(void * a);
int gingko_clnt_global_init(int argc, char *argv[]);

///gingko_common.h (18 matches)
void * helo_serv_s(void *, int);
void * join_job_s(void *, int);
void * quit_job_s(void *, int);
void * dead_host_s(void *, int);
void * get_blocks_s(void *, int);
void * g_none_s(void *, int);
void * new_host_s(void *, int);
void * del_host_s(void *, int);
void * erase_job_s(void *, int);

///seed.cpp (4 matches)
int init_struct(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info);
int file_counter(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info);
void init_total_count_size(s_job_t * jo);
int init_seed(s_job_t * jo);

///serv_main.cpp (6 matches)
int init_daemon(void);
inline void pthread_init();
inline void pthread_clean();
void serv_int_handler(const int sig);
void * serv_int_worker(void * a);
int gingko_serv_global_init(int argc, char *argv[]);

///path.cpp
char * cwd_path_to_abs_path(char * abs_path, const char * oldpath);

#endif /* UNITTEST */



