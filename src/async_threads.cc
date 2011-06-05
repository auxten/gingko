/*
 *  async_threads.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#include "gingko.h"
#include "async_pool.h"

extern struct event_base *ev_base;
char buffer[BUF_SIZ + 1];
static int curr_thread = 0;
static struct thread_worker *worker_list[MAX_TNUM];

void thread_worker_new(int id) {
	int ret;

	struct thread_worker *worker = (struct thread_worker *) malloc(
			sizeof(struct thread_worker));

	int fds[2];
	if (pipe(fds) != 0) {
		perror("pipe error");
		exit(1);
	}
	worker->notify_recv_fd = fds[0];
	worker->notify_send_fd = fds[1];

	worker->ev_base = event_init();
	if (!worker->ev_base) {
		perror("Worker event base initialize error");
		exit(1);
	}

	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setstacksize(&thread_attr, MYSTACKSIZE);
	//pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&worker->tid, &thread_attr, thread_worker_init,
			(void *) worker);
	if (ret) {
		perror("Thread create error");
		exit(1);
	}
	worker->id = id;
	worker_list[id] = worker;
	//printf("thread_worker_new :%d\n",worker_list[id]->notify_send_fd );
}

/* Worker initialize */
void * thread_worker_init(void *arg) {
	struct thread_worker *worker = (struct thread_worker *) arg;
	event_set(&worker->ev_notify, worker->notify_recv_fd, EV_READ | EV_PERSIST,
			thread_worker_process, worker);
	event_base_set(worker->ev_base, &worker->ev_notify);
	event_add(&worker->ev_notify, 0);
	event_base_loop(worker->ev_base, 0);

	return NULL;
}

/* Transfer a new event to worker */
void thread_worker_process(int fd, short ev, void *arg) {
	struct thread_worker *worker = (struct thread_worker *) arg;
	int c_id;
	read(fd, &c_id, sizeof(int));
	struct conn_client *client = conn_client_list_get(c_id);

	if (client->client_fd) {
		event_set(&client->ev_read, client->client_fd, EV_READ | EV_PERSIST,
				conn_tcp_server_on_data, (void *) client);
		event_base_set(worker->ev_base, &client->ev_read);
		if (-1 == event_add(&client->ev_read, 0)) {
			fprintf(stderr, "Cannot handle client's data event\n");
		}
	} else {
		fprintf(stderr, "conn_client_list_get error\n");
	}
	return;
}

int thread_list_find_next() {
	int i, tmp;
	for (i = 0; i < MAX_TNUM; i++) {
		tmp = (i + curr_thread + 1) % MAX_TNUM;
		if (worker_list[tmp] && worker_list[tmp]->tid) {
			curr_thread = tmp;
			return tmp;
		}
	}
	printf("thread pool full\n");
	return -1;
}

void thread_init() {
	int i;
	memset(worker_list, 0, sizeof(struct thread_worker *) * MAX_TNUM);
	for (i = 0; i < MAX_TNUM; i++) {
		thread_worker_new(i);
	}
}

/* Dispatch to worker */
void thread_worker_dispatch(int c_id) {
	int worker_id;
	int res;
	worker_id = thread_list_find_next();
	res = write(worker_list[worker_id]->notify_send_fd, &c_id, sizeof(int));
	if (res == -1) {
		fprintf(stderr, "Pipe write error\n");
	}
}
