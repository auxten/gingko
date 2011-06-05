/*
 *  threads.h
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#define MAX_TNUM  5
#define BUF_SIZ    255     // message buffer size

struct thread_worker
{
	int id;
	pthread_t tid;
	struct event_base *ev_base;
	struct event ev_notify;
	int notify_recv_fd;
	int notify_send_fd;
};

static struct thread_worker *worker_list[MAX_TNUM];
void thread_worker_new(int id);
void * thread_worker_init(void *arg);
void thread_worker_process(int fd, short ev, void *arg);
int thread_list_find_next();
void thread_worker_dispatch(int sig_id);
void thread_init();
