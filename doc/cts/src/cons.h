/*
 *  cons.h
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#define CLIENT_POOL_BASE_SIZE	15000
#define CLIENT_READ_BUFFER_SIZE 16

// Connection client
struct conn_client
{
	int id;
	int client_fd;
	unsigned long client_addr;
	int client_port;
	unsigned int conn_time;
	struct event ev_read;
	char *read_buffer;
	unsigned int buffer_size;
};
// Connection server struct
struct conn_server
{
	int listen_fd;
	struct sockaddr_in listen_addr;
	unsigned long srv_addr;
	int srv_port;
	unsigned int start_time;
	int nonblock;
	int listen_queue_length;
	int tcp_send_buffer_size;
	int tcp_recv_buffer_size;
	int tcp_timeout;
	int tcp_reuse;
	int tcp_nodelay;
	struct event ev_accept;
	void (* on_data_callback) (int, void *, unsigned int);
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
