/*
 *  gingko.h
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <pthread.h>

#include "event.h"

#ifndef GINGKO_H_
#define GINGKO_H_

using namespace std;
#define _FILE_OFFSET_BITS 			64

#define MAX_PATH_LEN                512
#define BLOCK_SIZE                  (1024*1024)
#define SERV_PORT                   2120  	// just like ZERO :p
#define REQ_QUE_LEN                 50
#define MIN_PORT                    1025    // Lowest port number
#define MAX_PORT                    65535 	// Highest port number
#define MAX_LIS_PORT                60000 	// Maximum listen port
#define MAX_PACKET_LEN              65536 	// Maximum packet length to be received
#define MAX_JOBS                    1024   	// for global locks
#define MAX_REQ_SERV_BLOCKS			20		// req at max MAX_REQ_SERV_BLOCKS from serv
#define MAX_URI                     MAX_PATH_LEN
#define MAX_INT64					9223372036854775807L
#define MAX_RETRY					3		// retry 3 times then fail
#define MAX_LONG_INT                19    	// int64_t int char
#define MYSTACKSIZE                 (10*1024*1024) // default at MacOS is 512k
#define CLIENT_POOL_BASE_SIZE       15000
#define CLIENT_READ_BUFFER_SIZE     256
#define FNV_HASH_TNUM				8
#define MAX_TNUM                    20
#define SEND_RETRY_INTERVAL         100     // microseconds
#define RCV_TIMEOUT					10		//  seconds
#define SND_TIMEOUT					5		//  seconds
#define LIMIT_UP_RATE               (10 * 1024 * 1024) //bytes per second
#define LIMIT_DOWN_RATE             (5 * 1024 * 1024) //bytes per second
#define BUF_SIZ                     255     // message buffer size
#define VNODE_NUM                   3       // every physical node have VNODE_NUM vnodes
#define CMD_COUNT					6       // to sizeof an extern array we must define it
#define MAX_SELECT_FD               1000
#define SERVER_IP                   "127.0.0.1" // for debug usage
#define IP_LEN                      17   	// IP char length
#define SERVER_LOG                  "../log/server.log"
#define CLIENT_LOG                  "../log/client.log"
#define CREATE_OPEN_FLAG            O_WRONLY|O_CREAT|O_TRUNC|O_NOCTTY
#define READ_OPEN_FLAG              O_RDONLY|O_NOFOLLOW|O_NOCTTY
#define WRITE_OPEN_FLAG             O_WRONLY|O_NOFOLLOW|O_NOCTTY
#define MSG_LEN                   	(MAX_URI + IP_LEN + 32 + MAX_LONG_INT * 2)
#define SHORT_MSG_LEN				(4 + 1 + IP_LEN + 1 + 5 + 1) //"NEWW 255.255.255.255 65535"
#define SERV_TRANS_TIME_PLUS		10000000 // 10s
#define JOIN_ERR                    1
#define MK_DIR_SYMLINK_ERR          2
#define DOWNLOAD_ERR                3

//job state
#define INITING						1  		//initializing
#define	INITED						2		//initialized
//lock state
#define LK_USING					1
#define LK_FREE						0

// getblock flag
#define W_DISK						0x01

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define BLOCK_COUNT(a) \
	(((a) % (BLOCK_SIZE)) ? ((a)/(BLOCK_SIZE) + 1) : ((a)/(BLOCK_SIZE)))
#ifndef GINGKO_OVERLOAD_S_HOST_LT
#define GINGKO_OVERLOAD_S_HOST_LT \
static bool operator <(const s_host& lhost, const s_host& rhost) {\
	return (((((u_int64_t) ntohl(inet_addr(lhost.addr))) << 16)\
			+ (u_int64_t) lhost.port) < ((((u_int64_t) ntohl(\
			inet_addr(rhost.addr))) << 16) + (u_int64_t) rhost.port));\
}
#endif /* GINGKO_OVERLOAD_S_HOST_LT */

#ifndef GINGKO_OVERLOAD_S_HOST_EQ
#define GINGKO_OVERLOAD_S_HOST_EQ \
static bool operator ==(const s_host& lhost, const s_host& rhost) {\
	return ( strncmp(lhost.addr, rhost.addr, sizeof(lhost.addr)) == 0 &&\
		lhost.port == rhost.port);\
}
#endif /* GINGKO_OVERLOAD_S_HOST_EQ */

/************** FUNC DICT **************/
typedef void * (*func_t)(void *, int);

//server func
void * helo_serv_s(void *, int);
void * join_job_s(void *, int);
void * quit_job_s(void *, int);
void * get_blocks_s(void *, int);
void * g_none_s(void *, int);
void * new_host_s(void * uri, int fd);
//client func
void * helo_serv_c(void *, int);
void * join_job_c(void *, int);
void * quit_job_c(void *, int);
void * get_blocks_c(void *, int);
void * g_none_c(void *, int);

/************** FUNC DICT **************/

#pragma pack (4)
typedef struct _s_file {
    struct stat f_stat;
    mode_t mode;
    int64_t size; // -1 for dir, -2 for symbol link
    char sympath[MAX_PATH_LEN];
    char name[MAX_PATH_LEN];
    unsigned char md5[16];
    unsigned char sha1[20];
} s_file;

typedef struct _s_host {
    char addr[IP_LEN];
    unsigned int port;
} s_host;

typedef struct _s_block {
    int64_t size;
    int64_t start_off;
    int64_t start_f;
    int done;
    unsigned char md5[16];
    unsigned int digest;
    set<s_host> * host_set; //lock here
} s_block;

typedef struct _s_job {
    int host_num;
    int lock_id;
    char uri[MAX_URI];
    char path[MAX_PATH_LEN];
    s_file * files;
    int64_t files_size;
    s_block * blocks;
    int64_t blocks_size;
    set<s_host> * host_set; //lock here
    int64_t file_count;
    int64_t block_count;
} s_job;

typedef struct _s_write_arg {
    int sent;
    int send_counter;
    int flag;
    int retry;
    int sz;
    char * p;
    struct event_base *ev_base;
    struct event ev_write;
} s_write_arg;

typedef struct _s_sendfile_arg {
    int64_t sent;
    u_int64_t send_counter;
    int in_fd;
    int retry;
    off_t offset;
    u_int64_t count;
    struct event_base *ev_base;
    struct event ev_write;
} s_gsendfile_arg;

typedef struct _s_vnode_download_arg {
    s_job * jo;
    int64_t blk_num;
    int64_t blk_count;
} s_vnode_download_arg;

typedef struct _s_host_hash_result {
    int64_t v_node[VNODE_NUM];
    int64_t length[VNODE_NUM];
} host_hash_result;

typedef struct _s_dir {
    int file_count;
    int init_s_file_iter;
    s_file * files;
    s_block * blocks;
    long init_s_block_iter;
    long total_size;
    long tmp_size;
    long last_init_block;
} s_dir;

typedef struct _s_lock {
    char state;
    pthread_rwlock_t lock;
} s_lock;

#pragma pack ()

inline int64_t next_f(s_job * jo, int64_t file) {
    do {
        file = (1 + file) % (jo->file_count);
    } while (((jo->files) + file)->size <= 0);
    return file;
}

inline int64_t next_b(s_job * jo, int64_t block) {
    return (++block) % (jo->block_count);
}

inline int64_t prev_b(s_job * jo, int64_t block) {
    return (block ? block - 1 : jo->block_count - 1);
}

inline u_int64_t host_distance(const s_host * h1, const s_host * h2) {
    return ((((u_int64_t) ntohl(inet_addr(h1->addr))) << 16)
            + (u_int64_t) h1->port)
            ^ ((((u_int64_t) ntohl(inet_addr(h2->addr))) << 16)
                    + (u_int64_t) h2->port);
}

void perr(const char *fmt, ...);
char * gettimestr(char * time);
int list_file(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info);
int file_counter(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info);
int recurse_dir(s_job *);
int sendall(int, const void *, int sz, int flag);
int sep_arg(char * inputstring, char * arg_array[], int max);
int parse_req(char *req);
struct hostent * gethostname_my(const char *host, struct hostent *hostbuf,
        char ** tmphstbuf, size_t hstbuflen);
int connect_host(s_host * h, int recv_sec, int send_sec);
int close_socket(int sock);
void bw_down_limit(int amount);
void bw_up_limit(int amount);
host_hash_result * host_hash(s_job * jo, const s_host * new_host,
        host_hash_result * result);
int sendblocks(int out_fd, s_job * jo, int64_t start, int64_t num);
int writeblock(s_job * jo, const unsigned char * buf, s_block * blk);
int digest_ok(void * buf, s_block * b);
void conn_send_data(int fd, void *str, unsigned int len);
int64_t getblock(s_host * ds, int64_t num, int64_t count, unsigned char flag,
        char * buf);
int broadcast_join(s_host * host_array, s_host *h);
int sendcmd(s_host *h, const char * cmd);

#endif /* GINGKO_H_ */
