/*
 *  async_pool.h
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/resource.h>
#include <pthread.h>
#include <errno.h>

#include "event.h"

struct thread_worker {
	int id;
	pthread_t tid;
	struct event_base *ev_base;
	struct event ev_notify;
	int notify_recv_fd;
	int notify_send_fd;
};

// Connection client
struct conn_client {
	int id;
	int client_fd;
	unsigned long client_addr;
	int client_port;
	unsigned int conn_time;
	func_t handle_client;
	struct event ev_read;
	char *read_buffer;
	unsigned int buffer_size;
};

// Connection server struct
struct conn_server {
	int listen_fd;
	struct sockaddr_in listen_addr;
	unsigned long srv_addr;
	int srv_port;
	unsigned int start_time;
	int nonblock;
	int listen_queue_length;
	int tcp_send_buffer_size;
	int tcp_recv_buffer_size;
	int send_timeout;
	int tcp_reuse;
	int tcp_nodelay;
	struct event ev_accept;
	void (* on_data_callback)(int, void *, unsigned int);
};

int conn_client_list_init();
int conn_tcp_server(struct conn_server *c);
void conn_tcp_server_accept(int fd, short ev, void *arg);
struct conn_client * add_new_conn_client(int client_fd);
void conn_tcp_server_on_data(int fd, short ev, void *arg);
int conn_client_list_find_free();
int conn_client_clear(struct conn_client *client);
int conn_client_free(struct conn_client *client);
struct conn_client * conn_client_list_get(int id);
int conn_setnonblock(int fd);
//void conn_send_data(int fd,void *str);
int conn_close();
//int base_init();
void thread_worker_new(int id);
void * thread_worker_init(void *arg);
void thread_worker_process(int fd, short ev, void *arg);
int thread_list_find_next();
void thread_worker_dispatch(int sig_id);
void thread_init();
void set_sig();

