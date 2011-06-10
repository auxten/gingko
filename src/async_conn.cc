/*
 *  async_conn.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-16.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#include "gingko.h"
#include "async_pool.h"

extern struct event_base *ev_base;
static int total_clients;
struct conn_client **client_list;
struct conn_server *server;

/* Initialization of client list */
int conn_client_list_init() {
    if ((client_list = (struct conn_client **) malloc(
            CLIENT_POOL_BASE_SIZE * sizeof(struct conn_client *))) == NULL) {
        fprintf(stderr, "Malloc error, cannot init client pool\n");
        exit(-1);
    }
    memset(client_list, 0, CLIENT_POOL_BASE_SIZE * sizeof(struct conn_client *));
    total_clients = 0;

    fprintf(stderr, "Client pool initialized as %d\n", CLIENT_POOL_BASE_SIZE);

    return CLIENT_POOL_BASE_SIZE;
}

/* Generate a TCP server by given struct */
int conn_tcp_server(struct conn_server *c) {
    if (server->srv_addr < 0) {
        server->srv_addr = INADDR_ANY;
    }

    if (server->srv_port > 65535) {
        exit(-1);
    }

    // If port number below 1024, root privilege needed
    if (server->srv_port <= 1024) {
        // CHECK ROOT PRIVILEGE
        if (0 != getuid() && 0 != geteuid()) {
            fprintf(stderr,
                    "Port %d number below 1024, root privilege needed\n",
                    server->srv_port);
            exit(-1);
        }
    }

    // Create new socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        fprintf(stderr, "Socket creation failed\n");
        exit(-1);
    }

    server->listen_addr.sin_family = AF_INET;
    server->listen_addr.sin_addr.s_addr = server->srv_addr;
    server->listen_addr.sin_port = htons(server->srv_port);

    // Bind socket
    if (bind(server->listen_fd, (struct sockaddr *) &server->listen_addr,
            sizeof(server->listen_addr)) < 0) {
        perr("Socket bind failed on port %lu:%d",
                server->srv_addr, server->srv_port);
        return -13;
    }

    // Listen socket
    if (listen(server->listen_fd, server->listen_queue_length) < 0) {
        perr("Socket listen failed");
        exit(-1);
    }

    // Set socket options
    struct timeval send_timeout =
    {
        tv_sec: server->send_timeout,
        tv_usec: 0
    };

    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &server->tcp_reuse,
            sizeof(server->tcp_reuse));
    setsockopt(server->listen_fd, SOL_SOCKET, SO_SNDTIMEO,
            (char *) &send_timeout, sizeof(struct timeval));
    setsockopt(server->listen_fd, SOL_SOCKET, SO_SNDBUF,
            &server->tcp_send_buffer_size, sizeof(server->tcp_send_buffer_size));
    setsockopt(server->listen_fd, SOL_SOCKET, SO_RCVBUF,
            &server->tcp_recv_buffer_size, sizeof(server->tcp_recv_buffer_size));
    setsockopt(server->listen_fd, IPPROTO_TCP, TCP_NODELAY,
            (char *) &server->tcp_nodelay, sizeof(server->tcp_nodelay));

    // Set socket non-blocking
    if (server->nonblock && conn_setnonblock(server->listen_fd) < 0) {
        fprintf(stderr, "Socket set non-blocking failed\n");
        exit(-1);
    }

    server->start_time = time((time_t *) NULL);

    //fprintf(stderr, "Socket server created on port %d\n", server->srv_port);
    //ev_base = event_init();
    // Add data handler
    event_set(&server->ev_accept, server->listen_fd, EV_READ | EV_PERSIST,
            conn_tcp_server_accept, (void *) c);
    event_base_set(ev_base, &server->ev_accept);
    event_add(&server->ev_accept, NULL);
    //event_base_loop(ev_base, 0);
    return server->listen_fd;
}

/* Accept new connection */
void conn_tcp_server_accept(int fd, short ev, void *arg) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct conn_client *client;
    //struct conn_server *server = (struct conn_server *) arg;
    // Accept new connection
    client_fd = accept(fd, (struct sockaddr *) &client_addr, &client_len);
    if (-1 == client_fd) {
        fprintf(stderr, "Accept error\n");
        return;
    }
    // Add connection to event queue
    client = add_new_conn_client(client_fd);
    if (!client) {
        fprintf(stderr, "Server limited: I cannot serve more clients\n");
        return;
    }
    // set blocking
    //fcntl(fd, F_SETFL, fcntl(fd, F_GETFL)& ~O_NONBLOCK);

    // Try to set non-blocking
    if (conn_setnonblock(client_fd) < 0) {
        conn_client_free(client);
        fprintf(stderr, "Client socket set non-blocking error\n");
        return;
    }

    // Client initialize
    client->client_addr = inet_addr(inet_ntoa(client_addr.sin_addr));
    client->client_port = client_addr.sin_port;
    client->conn_time = time((time_t *) NULL);
    thread_worker_dispatch(client->id);

    return;
}

/* Event on data from client */
void conn_tcp_server_on_data(int fd, short ev, void *arg) {
    struct conn_client *client = (struct conn_client *) arg;
    int res;
    unsigned int buffer_avail;
    int read_counter = 0;

    if (!client || !client->client_fd) {
        return;
    }

    if (fd != client->client_fd) {
        // Sanity
        conn_client_free(client);
        return;
    }

    if (!client->read_buffer) {
        // Initialize buffer
        client->read_buffer = (char *) malloc(CLIENT_READ_BUFFER_SIZE);
        client->buffer_size = buffer_avail = CLIENT_READ_BUFFER_SIZE;
        memset(client->read_buffer, 0, CLIENT_READ_BUFFER_SIZE);
    } else {
        buffer_avail = client->buffer_size;
    }

    while ((res = read(client->client_fd, client->read_buffer + read_counter,
            buffer_avail)) > 0) {
        //printf("res: %d\n",res);
        //printf("%s",client->read_buffer+read_counter);
        if (res > 0) {
            read_counter += res;
            client->buffer_size *= 2;
            client->read_buffer = (char *) realloc(client->read_buffer,
                    client->buffer_size);
            buffer_avail = client->buffer_size - read_counter;
            memset(client->read_buffer + read_counter, 0, buffer_avail);
            if (client->read_buffer == NULL) {
                perror("realloc error\n");
                conn_client_free(client);
                return;
            }
            continue;
        }
    }
    if (res < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            //#define EAGAIN      35      /* Resource temporarily unavailable */
            //#define EWOULDBLOCK EAGAIN      /* Operation would block */
            //#define EINPROGRESS 36      /* Operation now in progress */
            //#define EALREADY    37      /* Operation already in progress */
            printf("errno: %d", errno);
            perror("Socket read error\n");
        }
    }
    //printf("read_buffer:%s\n", client->read_buffer);//test
    if (server->on_data_callback) {
        server->on_data_callback(client->client_fd,
                (void *) client->read_buffer, sizeof(client->read_buffer));
    }
    conn_client_free(client);

    return;
}
/* ADD New connection client */
struct conn_client * add_new_conn_client(int client_fd) {
    int id;
    struct conn_client *tmp = (struct conn_client *) NULL;
    // Find a free slot
    id = conn_client_list_find_free();
    //printf("add_new_conn_client id %d\n",id);//test
    if (id >= 0) {
        tmp = client_list[id];
        if (!tmp) {
            tmp = (struct conn_client *) malloc(sizeof(struct conn_client));
        }
    } else {
        // FIXME
        //Client list pool full, if you want to enlarge it, modify async_pool.h source please
        fprintf(stderr, "Client list full\n");
        //return;
    }

    if (tmp) {
        memset(tmp, 0, sizeof(struct conn_client));
        conn_client_clear(tmp);
        tmp->id = id;
        tmp->client_fd = client_fd;
        client_list[id] = tmp;
        total_clients++;
    }
    return tmp;
}

/* Find a free slot from client pool */
int conn_client_list_find_free() {
    int i;

    for (i = 0; i < CLIENT_POOL_BASE_SIZE; i++) {
        if (!client_list[i] || 0 == client_list[i]->conn_time) {
            return i;
        }
    }

    return -1;
}

/* Close a client and free all data */
int conn_client_free(struct conn_client *client) {
    if (!client || !client->client_fd) {
        return -1;
    }
    //close socket and further receives will be disallowed
    shutdown(client->client_fd, SHUT_RD);
    close(client->client_fd);
    conn_client_clear(client);
    total_clients--;

    return 0;
}

/* Empty client struct data */
int conn_client_clear(struct conn_client *client) {
    if (client) {
        // Clear all data
        client->conn_time = 0;
        client->client_fd = 0;
        client->client_addr = 0;
        client->client_port = 0;
        client->buffer_size = 0;
        if (client->read_buffer) {
            free(client->read_buffer);
            client->read_buffer = (char *) NULL;
        }
        // Delete event
        event_del(&client->ev_read);
        return 0;
    }
    return -1;
}

/* Get client object from pool by given client_id */
struct conn_client * conn_client_list_get(int id) {
    return client_list[id];
}

/* Set non-blocking */
int conn_setnonblock(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return flags;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return -1;
    }

    return 0;
}

int conn_close() {
    int i;

    for (i = 0; i < CLIENT_POOL_BASE_SIZE; i++) {
        conn_client_free(client_list[i]);
    }

    shutdown(server->listen_fd, SHUT_RDWR);
    close(server->listen_fd);
    memset(server, 0, sizeof(struct conn_server));

    return 0;
}
