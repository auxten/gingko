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
#ifdef _BSD_MACHINE_TYPES_H_
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /* _BSD_MACHINE_TYPES_H_ */

#include <map>
#include <string>
#include <list>
#include <algorithm>

#include "gingko.h"
#include "async_pool.h"
#include "fnv_hash.h"
#include "path.h"

using namespace std;

/************** MUTEX & LOCK **************/
//server wide lock
pthread_rwlock_t grand_lock;
//job specific lock
s_lock job_lock[MAX_JOBS];
pthread_key_t dir_key;
/************** MUTEX & LOCK **************/

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT

/* Main event */
struct event_base *ev_base;
extern struct conn_server *server;

FILE * log_fp;
map<string, s_job> m_jobs;// jobs map


void * quit_job_s(void * uri, int fd) {
	printf("quit_job\n");
	return (void *)0;
}

void * g_none_s(void * uri, int fd) {
	printf("none\n");
	return (void *)0;
}

int broadcast_join(s_host * host_array, s_host *h) {
	s_host * p_h = host_array;
	char buf[SHORT_MSG_LEN];
	memset(buf, 0, SHORT_MSG_LEN);
	sprintf(buf, "NEWW\t%s\t%d", h->addr, h->port);
	while (p_h->port) {
		sendcmd(p_h, buf);
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
	if (fork() != 0)
		exit(1);

	if (setsid() < 0) {
		perror("setsid fail\n");
		exit(1);
	}
	//for ( fd=0; fd< 3; fd++) ;
	//close(fd);
	umask(0);
	time(&now);
	return 0;
}

static void sig_handler(const int sig) {
	fprintf(stderr, "\nSIGINT handled, server terminated\n");
	// Clear all status
	conn_close();
	exit(-1);
	return;
}

/* Set signal handler */
void set_sig() {
	struct sigaction *sa =
			(struct sigaction *) malloc(sizeof(struct sigaction));

	// SIGINT
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGHUP, sig_handler);

	/* Ignore terminal signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	/* Ignore SIGPIPE & SIGCLD */
	sa->sa_handler = SIG_IGN;
	sa->sa_flags = 0;
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	if (sigemptyset(&sa->sa_mask) == -1 || sigaction(SIGPIPE, sa, 0) == -1) {
		fprintf(stderr, "Ignore SIGPIPE failed\n");
		exit(-1);
	}

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
	pthread_key_create(&dir_key, NULL );
	pthread_rwlock_init(&grand_lock, NULL);
	for (int i=0; i < MAX_JOBS; i++) {
		job_lock[i].state = LK_FREE;
		pthread_rwlock_init(&(job_lock[i].lock), NULL);
	}
	return 0;
}

static inline int pthread_clean() {
	for (int i=0; i < MAX_JOBS; i++) {
		pthread_rwlock_destroy(&(job_lock[i].lock));
	}
	pthread_rwlock_destroy(&grand_lock);
	pthread_key_delete(dir_key);
	return 0;
}

int main(int argc, char *argv[]) {
	set_sig();
	if (!(log_fp = fopen(SERVER_LOG, "a+")))
		fprintf(stderr, "open log for appending failed, %s\n", strerror(errno));
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
