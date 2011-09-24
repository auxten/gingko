/**
 *  async_pool.h
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/
#ifndef ASYNC_POOL_H_
#define ASYNC_POOL_H_

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

/// Thread worker
struct thread_worker
{
    int id;
    pthread_t tid;
    struct event_base *ev_base;
    struct event ev_notify;
    int notify_recv_fd;
    int notify_send_fd;
};

/// Connection client
struct conn_client
{
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

/// Connection server struct
struct conn_server
{
    char is_server;
    int listen_fd;
    struct sockaddr_in listen_addr;
    in_addr_t srv_addr;
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

/// Accept new connection, start listen etc.
int conn_tcp_server(struct conn_server *c);
/// Accept new connection
void conn_tcp_server_accept(int fd, short ev, void *arg);
/// Accept new connection
struct conn_client * add_new_conn_client(int client_fd);
/// Event on data from client
void conn_tcp_server_on_data(int fd, short ev, void *arg);
/// Event on data from client
int conn_client_list_find_free();
/// Event on data from client
int conn_client_clear(struct conn_client *client);
/// Event on data from client
int conn_client_free(struct conn_client *client);
/// Get client object from pool by given client_id
struct conn_client * conn_client_list_get(int id);
/// close conn, shutdown && close
int conn_close();


/// close conn, shutdown && close
void * thread_worker_init(void *arg);
/// close conn, shutdown && close
void thread_worker_process(int fd, short ev, void *arg);
/// Dispatch to worker
void thread_worker_dispatch(int sig_id);
/// init the whole thread pool
int thread_init();
/// client side async server starter
void * gingko_clnt_async_server(void * arg);
/// server side async server starter
int gingko_serv_async_server();

#endif /** ASYNC_POOL_H_ **/
