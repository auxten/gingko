/**
 *  async_threads.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/

#include "gingko.h"
#include "async_pool.h"
#include "log.h"

static int g_curr_thread = 0;
static struct thread_worker ** g_worker_list;

extern s_gingko_global_t gko;

/**
 * @brief create new thread worker
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void thread_worker_new(int id)
{
    int ret;

    struct thread_worker *worker = new struct thread_worker;
    if(! worker)
    {
    	gko_log(FATAL, "new thread_worker failed");
    	exit(1);
    }

    int fds[2];
    if (pipe(fds) != 0)
    {
        gko_log(FATAL, "pipe error");
        exit(1);
    }
    worker->notify_recv_fd = fds[0];
    worker->notify_send_fd = fds[1];

    worker->ev_base = (event_base*)event_init();
    if (!worker->ev_base)
    {
        gko_log(FATAL, "Worker event base initialize error");
        exit(1);
    }

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setstacksize(&thread_attr, MYSTACKSIZE);
    ///pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&worker->tid, &thread_attr, thread_worker_init,
            (void *) worker);
    if (ret)
    {
        gko_log(FATAL, "Thread create error");
        exit(1);
    }
    worker->id = id;
    *(g_worker_list + id) = worker;
    ///gko_log(NOTICE, "thread_worker_new :%d",(*(g_worker_list+id))->notify_send_fd );
}

/**
 * @brief Worker initialize
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * thread_worker_init(void *arg)
{
    struct thread_worker *worker = (struct thread_worker *) arg;
    event_set(&worker->ev_notify, worker->notify_recv_fd, EV_READ | EV_PERSIST,
            thread_worker_process, worker);
    event_base_set(worker->ev_base, &worker->ev_notify);
    event_add(&worker->ev_notify, 0);
    event_base_loop(worker->ev_base, 0);

    return NULL;
}

/**
 * @brief Transfer a new event to worker
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void thread_worker_process(int fd, short ev, void *arg)
{
    struct thread_worker *worker = (struct thread_worker *) arg;
    int c_id;
    read(fd, &c_id, sizeof(int));
    struct conn_client *client = conn_client_list_get(c_id);

    if (client->client_fd)
    {
        event_set(&client->ev_read, client->client_fd, EV_READ | EV_PERSIST,
                conn_tcp_server_on_data, (void *) client);
        event_base_set(worker->ev_base, &client->ev_read);
        if (-1 == event_add(&client->ev_read, 0))
        {
            gko_log(WARNING, "Cannot handle client's data event");
        }
    }
    else
    {
        gko_log(WARNING, "conn_client_list_get error");
    }
    return;
}

/**
 * @brief find an availiable thread, return thread index; on error
 *          return -1
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int thread_list_find_next()
{
    int i;
    int tmp;

    for (i = 0; i < gko.opt.worker_thread; i++)
    {
        tmp = (i + g_curr_thread + 1) % gko.opt.worker_thread;
        if (*(g_worker_list + tmp) && (*(g_worker_list + tmp))->tid)
        {
            g_curr_thread = tmp;
            return tmp;
        }
    }
    gko_log(WARNING, "thread pool full");
    return -1;
}

/**
 * @brief init the whole thread pool
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void thread_init()
{
    int i;
    g_worker_list = new struct thread_worker *[gko.opt.worker_thread];
    if (! g_worker_list)
    {
        gko_log(FATAL, "new new struct thread_worker *[gko.opt.worker_thread] failed");
        exit(1);
    }
    memset(g_worker_list, 0, sizeof(struct thread_worker *) * gko.opt.worker_thread);
    for (i = 0; i < gko.opt.worker_thread; i++)
    {
        thread_worker_new(i);
    }
}

/**
 * @brief Dispatch to worker
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void thread_worker_dispatch(int c_id)
{
    int worker_id;
    int res;
    worker_id = thread_list_find_next();
    if (worker_id < 0)
    {
        gko_log(WARNING, "can't find available thread");
        return;
    }
    res = write((*(g_worker_list + worker_id))->notify_send_fd, &c_id,
            sizeof(int));
    if (res == -1)
    {
        gko_log(WARNING, "Pipe write error");
    }
}
