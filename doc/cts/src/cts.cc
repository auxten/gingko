/*
 *  cts.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#include "cts.h"

/* Main event */
struct event_base *ev_base;
extern struct conn_server *server;

static void sig_handler(const int sig)
{
	fprintf(stderr, "\nSIGINT handled, server terminated\n");
	// Clear all status
	conn_close();
    exit(-1);
	return;
}

/* Set signal handler */
void set_sig()
{
	struct sigaction *sa = (struct sigaction *) malloc(sizeof(struct sigaction));
	
	// SIGINT
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGHUP, sig_handler);

	// Ignore SIGPIPE & SIGCLD
	sa->sa_handler = SIG_IGN;
	sa->sa_flags = 0;
	signal(SIGPIPE, SIG_IGN);
	if (sigemptyset(&sa->sa_mask) == -1 || sigaction(SIGPIPE, sa, 0) == -1)
	{
		fprintf(stderr, "Ignore SIGPIPE failed\n");
		exit(-1);
	}

	return;
}
void conn_send_data(int fd,void *str,unsigned int len)
{
	char * p = (char *) str;
	printf("read_buffer:%s\n", p);
    /*if(send(fd, str, strlen(str),0)==-1)
    {
        perror("sending error!");
    }*/
    return;
}

int base_init()
{
	server = (struct conn_server *) malloc(sizeof(struct conn_server));
	memset(server, 0, sizeof(struct conn_server));
	//server->srv_addr = inet_addr("0.0.0.0");
	server->srv_addr = htons(INADDR_ANY);
	server->srv_port = 6666;
	server->listen_queue_length = 4096;
	server->nonblock = 1;
	server->tcp_nodelay = 1;
	server->tcp_reuse = 1;
	server->tcp_send_buffer_size = 131072;
	server->tcp_recv_buffer_size = 131072;
	server->tcp_timeout = 3;
	server->on_data_callback = conn_send_data;
	// A new TCP server
	conn_tcp_server(server);
	return 0;
}

int main(int argc, char *argv[])
{
	set_sig();
    // Event initialize
	ev_base = event_init();
    conn_client_list_init();
    thread_init();
	base_init();// haved event_base
	event_base_loop(ev_base, 0);
	return 0;
}
