/*
 *  gingko_serv.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */
#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /* GINGKO_SERV */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /* __APPLE__ */

#include <map>
#include <string>
#include <list>
#include <algorithm>

#include "gingko.h"
#include "async_pool.h"
#include "fnv_hash.h"
#include "path.h"

using namespace std;

/************** PTHREAD STUFF **************/
//server wide lock
pthread_rwlock_t grand_lock;
//job specific lock
s_lock job_lock[MAX_JOBS];
pthread_key_t dir_key;
//mutex for bandwidth limit
pthread_mutex_t bw_up_mutex;
pthread_mutex_t bw_down_mutex;
/************** PTHREAD STUFF **************/

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT

/* Main event */
struct event_base *ev_base;
extern struct conn_server *server;

FILE * log_fp;
map<string, s_job> m_jobs;// jobs map


void * g_none_s(void * uri, int fd) {
    printf("none\n");
    return (void *) 0;
}

int erase_job(string &uri_string) {
    extern map<string, s_job> m_jobs;// jobs map
    extern pthread_rwlock_t grand_lock;
    extern s_lock job_lock[MAX_JOBS];
    map<string, s_job>::iterator it;
    pthread_rwlock_rdlock(&grand_lock);
    if ((it = m_jobs.find(uri_string)) == m_jobs.end()) {
        return -1;
    }
    s_job *p = &(it->second);
    pthread_rwlock_unlock(&grand_lock);
    pthread_rwlock_wrlock(&job_lock[p->lock_id].lock);
    if(p->blocks) {
        free(p->blocks);
        p->blocks = NULL;
    }
    if(p->files) {
        free(p->files);
        p->files = NULL;
    }
    if(p->host_set) {
        delete p->host_set;
        p->host_set = NULL;
    }
    pthread_rwlock_unlock(&job_lock[p->lock_id].lock);
    /*
     * for safety reinit the rwlock
     */
    pthread_rwlock_destroy(&(job_lock[p->lock_id].lock));
    pthread_rwlock_init(&(job_lock[p->lock_id].lock), NULL);
    job_lock[p->lock_id].state = LK_FREE;

    m_jobs.erase(uri_string);
    return 0;
}

int broadcast_join(s_host * host_array, s_host *h) {
    s_host * p_h = host_array;
    char buf[SHORT_MSG_LEN] = {'\0'};
    sprintf(buf, "NEWW\t%s\t%d", h->addr, h->port);
    while (p_h->port) {
        sendcmd(p_h, buf, 2, 2);
        p_h++;
    }
    return 0;
}

int init_daemon(void) {
    time_t now;
    /* Return if already daemon */
    if (getppid() == 1)
        return 1;
    /* Ignore terminal signals */
    set_sig();
    /* Exit the parent process and background the child*/
    ASSERT(fork() == 0);

    ASSERT(setsid() > 0);
    //for ( fd=0; fd< 3; fd++) ;
    //close(fd);
    umask(0);
    time(&now);
    return 0;
}

static void int_handler(const int sig) {
    fprintf(stderr, "\nSIGNAL handled, server terminated\n");
    // Clear all status
    conn_close();
    exit(-1);
}


/* Set signal handler */
void set_sig() {
    struct sigaction *sa =
            (struct sigaction *) malloc(sizeof(struct sigaction));

    // SIGINT
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    signal(SIGQUIT, int_handler);
    signal(SIGKILL, int_handler);
    signal(SIGHUP, int_handler);

    /* Ignore terminal signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    /* Ignore SIGPIPE & SIGCLD */
    sa->sa_handler = SIG_IGN;
    sa->sa_flags = 0;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    ASSERT(sigemptyset(&sa->sa_mask) != -1 && sigaction(SIGPIPE, sa, 0) != -1);
    return;
}

int base_init() {
    server = (struct conn_server *) malloc(sizeof(struct conn_server));
    memset(server, 0, sizeof(struct conn_server));
    //server->srv_addr = inet_addr("0.0.0.0");
    server->srv_addr = htons(INADDR_ANY);
    server->srv_port = SERV_PORT;
    server->listen_queue_length = REQ_QUE_LEN;
    server->nonblock = 1;
    server->tcp_nodelay = 1;
    server->tcp_reuse = 1;
    server->tcp_send_buffer_size = 131072;
    server->tcp_recv_buffer_size = 131072;
    server->send_timeout = SND_TIMEOUT;
    server->on_data_callback = conn_send_data;
    // A new TCP server
    conn_tcp_server(server);
    return 0;
}

static inline int pthread_init() {
    pthread_key_create(&dir_key, NULL);
    pthread_rwlock_init(&grand_lock, NULL);
    pthread_mutex_init(&bw_up_mutex, NULL);
    pthread_mutex_init(&bw_down_mutex, NULL);
    for (int i = 0; i < MAX_JOBS; i++) {
        job_lock[i].state = LK_FREE;
        pthread_rwlock_init(&(job_lock[i].lock), NULL);
    }
    return 0;
}

static inline int pthread_clean() {
    for (int i = 0; i < MAX_JOBS; i++) {
        pthread_rwlock_destroy(&(job_lock[i].lock));
    }
    pthread_mutex_destroy(&bw_up_mutex);
    pthread_mutex_destroy(&bw_down_mutex);
    pthread_rwlock_destroy(&grand_lock);
    pthread_key_delete(dir_key);
    return 0;
}

int main(int argc, char *argv[]) {
    set_sig();
    if (!(log_fp = fopen(SERVER_LOG, "a+")))
        fprintf(stderr, "open log for appending failed");
    //printf("%lld^^^^\n", sizeof(cmd_list));
    //printf("int:%d, int64_t:%d, int64_t:%d\n",
    //          sizeof(int), sizeof(int64_t), sizeof(int64_t));
    //printf("addr1:%d, addr2:%d\n", inet_addr("10.1.1.1"), inet_addr("9.1.1.1"));
    //printf("--%d-\n",
    //                  (((u_int64_t)ntohl(inet_addr("10.1.1.11"))) << 16 + 1) <
    //                  (((u_int64_t)ntohl(inet_addr("10.1.1.10"))) << 16 + 1));
    //init_daemon();
    // pthread initialize
    pthread_init();
    // Event initialize
    ev_base = event_init();
    conn_client_list_init();
    thread_init();
    base_init();// haved event_base
    event_base_loop(ev_base, 0);

    pthread_clean();
    //server(arg);
    //recurse_dir(argc, argv);
}
