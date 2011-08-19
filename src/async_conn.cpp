/**
 *  async_conn.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/

#include "gingko.h"
#include "async_pool.h"
#include "log.h"
#include "socket.h"

extern s_gingko_global_t gko;

static struct event_base *g_ev_base;
static int g_total_clients;
static struct conn_client **g_client_list;
static struct conn_server *g_server;


/**
 * @brief Initialization of client list
 *
 * @see
 * @note
 * @author wangpengcheng01
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int conn_client_list_init()
{
    if ((g_client_list = (struct conn_client **) malloc(
            gko.opt.connlimit * sizeof(struct conn_client *))) == NULL)
    {
        gko_log(FATAL, "Malloc error, cannot init client pool");
        exit(-1);
    }
    memset(g_client_list, 0, gko.opt.connlimit * sizeof(struct conn_client *));
    g_total_clients = 0;

    gko_log(NOTICE, "Client pool initialized as %d", gko.opt.connlimit);

    return gko.opt.connlimit;
}

/**
 * @brief Init for gingko_serv
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int gingko_serv_async_server_base_init()
{
    g_server = (struct conn_server *) malloc(sizeof(struct conn_server));
    memset(g_server, 0, sizeof(struct conn_server));
    g_server->srv_addr = gko.opt.bind_ip;
    g_server->srv_port = gko.opt.port;
    g_server->listen_queue_length = REQ_QUE_LEN;
    g_server->nonblock = 1;
    g_server->tcp_nodelay = 1;
    g_server->tcp_reuse = 1;
    g_server->tcp_send_buffer_size = TCP_BUF_SZ;
    g_server->tcp_recv_buffer_size = TCP_BUF_SZ;
    g_server->send_timeout = SND_TIMEOUT;
    g_server->on_data_callback = conn_send_data;
    g_server->is_server = 1;
    /// A new TCP server
    conn_tcp_server(g_server);
    return 0;
}

/**
 * @brief Init for gingko_clnt
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int gingko_clnt_async_server_base_init(s_host_t * the_host)
{
    int port = MAX_LIS_PORT;
    g_server = (struct conn_server *) malloc(sizeof(struct conn_server));
    memset(g_server, 0, sizeof(struct conn_server));
    g_server->srv_addr = gko.opt.bind_ip;
    g_server->srv_port = port;
    g_server->listen_queue_length = REQ_QUE_LEN;
    g_server->nonblock = 1;
    g_server->tcp_nodelay = 1;
    g_server->tcp_reuse = 1;
    g_server->tcp_send_buffer_size = TCP_BUF_SZ;
    g_server->tcp_recv_buffer_size = TCP_BUF_SZ;
    g_server->send_timeout = SND_TIMEOUT;
    g_server->on_data_callback = conn_send_data;
    g_server->is_server = 0;
    /// A new TCP server
    while (conn_tcp_server(g_server) == BIND_FAIL)
    {
        if (port < MIN_PORT)
        {
            gko_log(FATAL, "bind port failed, last try is %d", port);
            return -1;
        }
        g_server->srv_port = --port;
        usleep(BIND_INTERVAL);
    }
    the_host->port = g_server->srv_port;
    return 0;
}

/**
 * @brief Generate a TCP server by given struct
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int conn_tcp_server(struct conn_server *c)
{
    if (g_server->srv_port > MAX_PORT)
    {
        exit(-1);
    }

    /// If port number below 1024, root privilege needed
    if (g_server->srv_port < MIN_PORT)
    {
        /// CHECK ROOT PRIVILEGE
        if (0 != getuid() && 0 != geteuid())
        {
            gko_log(FATAL, "Port %d number below 1024, root privilege needed",
                    g_server->srv_port);
            exit(-1);
        }
    }

    /// Create new socket
    g_server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server->listen_fd < 0)
    {
        gko_log(WARNING, "Socket creation failed");
        exit(-1);
    }

    g_server->listen_addr.sin_family = AF_INET;
    g_server->listen_addr.sin_addr.s_addr = g_server->srv_addr;
    g_server->listen_addr.sin_port = htons(g_server->srv_port);

    /// Bind socket
    if (bind(g_server->listen_fd, (struct sockaddr *) &g_server->listen_addr,
            sizeof(g_server->listen_addr)) < 0)
    {
        if (g_server->is_server)
        {
            gko_log(FATAL, "Socket bind failed on port, server exit");
            exit(-1);
        }
        else
        {
            gko_log(WARNING, "Socket bind failed on port");
            return BIND_FAIL;
        }
    }

    /// Listen socket
    if (listen(g_server->listen_fd, g_server->listen_queue_length) < 0)
    {
        gko_log(FATAL, "Socket listen failed");
        exit(-1);
    }

    /// Set socket options
    struct timeval send_timeout;
    send_timeout.tv_sec = g_server->send_timeout;
    send_timeout.tv_usec = 0;

    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &g_server->tcp_reuse,
            sizeof(g_server->tcp_reuse));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_SNDTIMEO,
            (char *) &send_timeout, sizeof(struct timeval));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_SNDBUF,
            &g_server->tcp_send_buffer_size, sizeof(g_server->tcp_send_buffer_size));
    setsockopt(g_server->listen_fd, SOL_SOCKET, SO_RCVBUF,
            &g_server->tcp_recv_buffer_size, sizeof(g_server->tcp_recv_buffer_size));
    setsockopt(g_server->listen_fd, IPPROTO_TCP, TCP_NODELAY,
            (char *) &g_server->tcp_nodelay, sizeof(g_server->tcp_nodelay));

    /// Set socket non-blocking
    if (g_server->nonblock && setnonblock(g_server->listen_fd) < 0)
    {
        gko_log(WARNING, "Socket set non-blocking failed");
        exit(-1);
    }

    g_server->start_time = time((time_t *) NULL);

    ///gko_log(WARNING, "Socket server created on port %d", server->srv_port);
    ///g_ev_base = event_init();
    /// Add data handler
    event_set(&g_server->ev_accept, g_server->listen_fd, EV_READ | EV_PERSIST,
            conn_tcp_server_accept, (void *) c);
    event_base_set(g_ev_base, &g_server->ev_accept);
    event_add(&g_server->ev_accept, NULL);
    ///event_base_loop(g_ev_base, 0);
    return g_server->listen_fd;
}

/**
 * @brief Accept new connection
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void conn_tcp_server_accept(int fd, short ev, void *arg)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct conn_client *client;
    ///struct conn_server *server = (struct conn_server *) arg;
    /// Accept new connection
    client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_len);
    if (-1 == client_fd)
    {
        gko_log(FATAL, "Accept error");
        return;
    }
    /// Add connection to event queue
    client = add_new_conn_client(client_fd);
    if (!client)
    {
        ///close socket and further receives will be disallowed
        shutdown(client_fd, SHUT_RD);
        close(client_fd);
        gko_log(WARNING, "Server limited: I cannot serve more clients");
        return;
    }
    /// set blocking
    ///fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)& ~O_NONBLOCK);

    /// Try to set non-blocking
    if (setnonblock(client_fd) < 0)
    {
        conn_client_free(client);
        gko_log(FATAL, "Client socket set non-blocking error");
        return;
    }

    /// Client initialize
    client->client_addr = inet_addr(inet_ntoa(client_addr.sin_addr));
    client->client_port = client_addr.sin_port;
    client->conn_time = time((time_t *) NULL);
    thread_worker_dispatch(client->id);

    return;
}

/**
 * @brief Before send data all req to server will filtered by conn_send_data
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void conn_send_data(int fd, void *str, unsigned int len)
{
    int i;
    char * p = (char *) str;
    i = parse_req(p);
    if (i != 0)
    {
        gko_log(NOTICE, "got req: %s, index: %d", p, i);
    }
    (*gko.func_list_p[i])(p, fd);
    ///gko_log(NOTICE, "read_buffer:%s", p);
    return;
}

/**
 * @brief Event on data from client
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void conn_tcp_server_on_data(int fd, short ev, void *arg)
{
    struct conn_client *client = (struct conn_client *) arg;
    int res;
    unsigned int buffer_avail;
    int read_counter = 0;

    if (!client || !client->client_fd)
    {
        return;
    }

    if (fd != client->client_fd)
    {
        /// Sanity
        conn_client_free(client);
        return;
    }

    if (!client->read_buffer)
    {
        /// Initialize buffer
        client->read_buffer = (char *) malloc(CLNT_READ_BUFFER_SIZE);
        client->buffer_size = buffer_avail = CLNT_READ_BUFFER_SIZE;
        memset(client->read_buffer, 0, CLNT_READ_BUFFER_SIZE);
    }
    else
    {
        buffer_avail = client->buffer_size;
    }

    while ((res = read(client->client_fd, client->read_buffer + read_counter,
            buffer_avail)) > 0)
    {
        ///gko_log(NOTICE, "res: %d",res);
        ///gko_log(NOTICE, "%s",client->read_buffer+read_counter);
        read_counter += res;
        client->buffer_size *= 2;
        client->read_buffer = (char *) realloc(client->read_buffer,
                client->buffer_size);
        buffer_avail = client->buffer_size - read_counter;
        memset(client->read_buffer + read_counter, 0, buffer_avail);
        if (client->read_buffer == NULL)
        {
            gko_log(FATAL, "realloc error");
            conn_client_free(client);
            return;
        }
        continue;
    }
    if (res < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            ///#define EAGAIN      35      /** Resource temporarily unavailable **/
            ///#define EWOULDBLOCK EAGAIN      /** Operation would block **/
            ///#define EINPROGRESS 36      /** Operation now in progress **/
            ///#define EALREADY    37      /** Operation already in progress **/
            gko_log(WARNING, "Socket read error");
        }
    }
    ///gko_log(NOTICE, "read_buffer:%s", client->read_buffer);///test
    if (g_server->on_data_callback)
    {
        g_server->on_data_callback(client->client_fd,
                (void *) client->read_buffer, sizeof(client->read_buffer));
    }
    conn_client_free(client);

    return;
}

/**
 * @brief ADD New connection client
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
struct conn_client * add_new_conn_client(int client_fd)
{
    int id;
    struct conn_client *tmp = (struct conn_client *) NULL;
    /// Find a free slot
    id = conn_client_list_find_free();
    ///gko_log(NOTICE, "add_new_conn_client id %d",id);///test
    if (id >= 0)
    {
        tmp = g_client_list[id];
        if (!tmp)
        {
            tmp = (struct conn_client *) malloc(sizeof(struct conn_client));
        }
    }
    else
    {
        /// FIXME
        ///Client list pool full, if you want to enlarge it, modify async_pool.h source please
        gko_log(WARNING, "Client list full");
        ///return;
    }

    if (tmp)
    {
        memset(tmp, 0, sizeof(struct conn_client));
        conn_client_clear(tmp);
        tmp->id = id;
        tmp->client_fd = client_fd;
        g_client_list[id] = tmp;
        g_total_clients++;
    }
    return tmp;
}

/**
 * @brief Find a free slot from client pool
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int conn_client_list_find_free()
{
    int i;

    for (i = 0; i < gko.opt.connlimit; i++)
    {
        if (!g_client_list[i] || 0 == g_client_list[i]->conn_time)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief Close a client and free all data
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int conn_client_free(struct conn_client *client)
{
    if (!client || !client->client_fd)
    {
        return -1;
    }
    ///close socket and further receives will be disallowed
    shutdown(client->client_fd, SHUT_RD);
    close(client->client_fd);
    conn_client_clear(client);
    g_total_clients--;

    return 0;
}

/**
 * @brief Empty client struct data
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int conn_client_clear(struct conn_client *client)
{
    if (client)
    {
        /// Clear all data
        client->conn_time = 0;
        client->client_fd = 0;
        client->client_addr = 0;
        client->client_port = 0;
        client->buffer_size = 0;
        if (client->read_buffer)
        {
            free(client->read_buffer);
            client->read_buffer = (char *) NULL;
        }
        /// Delete event
        event_del(&client->ev_read);
        return 0;
    }
    return -1;
}

/**
 * @brief Get client object from pool by given client_id
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
struct conn_client * conn_client_list_get(int id)
{
    return g_client_list[id];
}

/**
 * @brief close conn, shutdown && close
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int conn_close()
{

    for (int i = 0; i < gko.opt.connlimit; i++)
    {
        conn_client_free(g_client_list[i]);
    }

    shutdown(g_server->listen_fd, SHUT_RDWR);
    close(g_server->listen_fd);
    memset(g_server, 0, sizeof(struct conn_server));

    return 0;
}

/**
 * @brief client side async server starter
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * gingko_clnt_async_server(void * arg)
{
    s_async_server_arg_t * arg_p = (s_async_server_arg_t *) arg;
    g_ev_base = event_init();
    if (!g_ev_base)
    {
        gko_log(FATAL, "event init failed");
        exit(1);
    }

    if (gingko_clnt_async_server_base_init(arg_p->s_host_p))
    {
        gko_log(FATAL, "gingko_clnt_async_server_base_init failed");
        exit(1);
    }

    if (conn_client_list_init() < 1)
    {
        gko_log(FATAL, "conn_client_list_init failed");
        exit(1);
    }
    thread_init();
    event_base_loop(g_ev_base, 0);
    pthread_exit((void *) 0);
}

/**
 * @brief server side async server starter
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int gingko_serv_async_server()
{
    g_ev_base = event_init();
    if (!g_ev_base)
    {
        gko_log(FATAL, "event init failed");
        exit(1);
    }

    if (gingko_serv_async_server_base_init())
    {
        gko_log(FATAL, "gingko_clnt_async_server_base_init failed");
        exit(1);
    }

    if (conn_client_list_init() < 1)
    {
        gko_log(FATAL, "conn_client_list_init failed");
        exit(1);
    }
    thread_init();
    event_base_loop(g_ev_base, 0);

    return 0;
}

