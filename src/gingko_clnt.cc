/*
 *  gingko_clnt.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-10.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */
#ifndef GINGKO_CLNT
#define GINGKO_CLNT
#endif /* GINGKO_CLNT */

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

#include "gingko.h"
#include "async_pool.h"
#include "fnv_hash.h"
#include "path.h"
#include "route.h"

/************** PTHREAD STUFF **************/
pthread_attr_t attr;
//client wide lock
pthread_rwlock_t clnt_lock;
//block host set lock
pthread_rwlock_t blk_hostset_lock;
//mutex for hosts_noready
pthread_mutex_t noready_mutex;
//mutex for bandwidth limit
pthread_mutex_t bw_up_mutex;
pthread_mutex_t bw_down_mutex;
/************** PTHREAD STUFF **************/

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT
GINGKO_OVERLOAD_S_HOST_EQ

using namespace std;
FILE * log_fp;
// ready_to_serv flog
char ready_to_serv = 0;

static struct hostent *serv;
s_job job;
vector<s_host> hosts_noready; //save the NEWWed host when server is not ready
//eclipse thought the below is incorrect, so auto complete will not work
//s_host the_host = {addr: "\0", port: 0};
s_host the_host, the_server;
char localpath[MAX_PATH_LEN];
struct sockaddr_in seed_addr;//source addr_in
/* Main event */
struct event_base *ev_base;
extern struct conn_server *server;

/*
 * CORE algrithm: hash the host to the data ring
 * if result is NULL, the host_hash_result is no need
 */
host_hash_result * host_hash(s_job * jo, const s_host * new_host,
        host_hash_result * result) {
    set<s_host> * h_set;
    if (jo->block_count == 0) {
        return NULL;
    }
    if (result) {
        memset(result, 0, sizeof(host_hash_result));
    }
    u_int64_t host_key = (((u_int64_t) inet_addr(new_host->addr)) << 32)
            + (new_host->port) << 16;
    u_int64_t vnode_distance =
            (jo->block_count / VNODE_NUM / 3) ? ((u_int64_t) fnv_hash(
                    (void *) new_host, sizeof(s_host), 0) % (jo->block_count
                    / VNODE_NUM / 3) + (jo->block_count * 2 / VNODE_NUM / 3))
                    : 1;
    printf("fnv_hash: %lld, ip: %s, port: %d\n",
            (u_int64_t) fnv_hash((void *) new_host, sizeof(s_host), 0),
            new_host->addr, new_host->port);
    printf("fnv_hash: %lld, ip: %s, port: %d\n",
            (u_int64_t) fnv_hash((void *) new_host, sizeof(s_host), 0),
            new_host->addr, new_host->port);
    u_int64_t vnode_start = host_key % (jo->block_count);
    printf("vnode_distance:%lld, vnode_start:%lld\n", vnode_distance,
            vnode_start);
    for (int i = 0; i < VNODE_NUM; i++) {
        int64_t vnode = (i * vnode_distance + vnode_start) % (jo->block_count);
        pthread_rwlock_wrlock(&blk_hostset_lock);
        h_set = ((jo->blocks) + vnode)->host_set;
        if (!h_set) {
            h_set = ((jo->blocks) + vnode)->host_set = new set<s_host> ;
        }
        (*h_set).insert(*new_host);
        pthread_rwlock_unlock(&blk_hostset_lock);

        if (result) {
            /*
             *  check if the v_node fall in the same block
             *  this must happen when block_count < 3
             */
            for (int j = 0; j < i; j++) {
                if (vnode == result->v_node[j]) {
                    vnode = -1; // mark the node unavailable
                    break;
                }
            }
            result->v_node[i] = vnode;
        }
        printf("vnode: %lld\n", vnode);
    }
    printf("host_key:%lld\n", host_key);
    return result;
}

int64_t getblock(s_host * dhost, int64_t num, int64_t count,
        unsigned char flag, char * buf) {
    int sock;
    int64_t i, j;
    ssize_t conuter;
    char msg[MSG_LEN];
    char * arg_array[4];
    sock = connect_host(dhost, RCV_TIMEOUT, SND_TIMEOUT);
    if (sock < 0) {
        perr("connect_host error\n");
        return 0;
    }
    /*
     * send "GETT uri start num" to server
     */
    snprintf(msg, MSG_LEN, "GETT\t%s\t%lld\t%lld", job.uri, num, count);
    if ((j = sendall(sock, msg, MSG_LEN, 0)) < 0) {
        printf("sent: %lld\n", j);
        perror("sending GETT error!");
        return 0;
    }
    printf("Sent: %s, Length: %ld\n", msg, strlen(msg));
    /*
     * server will send "HAVA n"
     */
    j = recv(sock, msg, sizeof(msg), MSG_WAITALL);
    printf("Got: %lld ##%s##\n", j, msg);
    sep_arg(msg, arg_array, 4);
    int64_t n_to_recv = atol(arg_array[1]);
    printf("arg1: %lld\n", n_to_recv);
    /*
     * recv data and check the digest
     */
    s_block * b;
    for (i = 0; i < n_to_recv; i++) {
        b = job.blocks + (num + i) % job.block_count;
        conuter = recv(sock, buf, b->size, MSG_WAITALL);
        fprintf(stderr, "%d gotfile: %d \n", i, conuter);
        //fprintf(stderr, "buf: %s \n", buf);// test digest err
        //        if (i == 0) {
        //        	*buf += 1;
        //        }
        if (digest_ok(buf, b)) {
            //fprintf(stderr, "digest_ok\n");
            if (flag & W_DISK) {
                if (writeblock(&job, (unsigned char *) buf, b) < 0) {
                    return -1;
                } else {
                    b->done = 1;
                }
            }
        } else {
            fprintf(stderr, "digest_err\n");
            break;
        }
        //usleep(100);
        bw_down_limit(conuter);
    }
    close_socket(sock);
    return i;
}

void * helo_serv_c(void * arg, int fd) {
    int sock;
    int j;
    char msg[2]; //for recv HI, must not larger than 2
    struct sockaddr_in channel;
    struct sockaddr addr;//source addr
    struct timeval recv_timeout;
    recv_timeout.tv_sec = RCV_TIMEOUT;
    recv_timeout.tv_usec = 0;

    socklen_t sock_len = sizeof(addr);

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        perr("get socket error\n");
        return (void *) 1;
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, serv->h_addr, serv->h_length);
    channel.sin_port = htons(SERV_PORT);
    //connect and send the msg
    if (0 > connect(sock, (struct sockaddr *) &channel, sizeof(channel))) {
        perr("connect error\n");
    }
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout,
            sizeof(struct timeval));

    //get the sockaddr and port
    // say HELO expect HI
    sendall(sock, "HELO", 4, 0);
    j = recv(sock, msg, sizeof(msg), MSG_WAITALL);
    if (-1 == getsockname(sock, &addr, &sock_len)) {
        perror("getsockname error");
        return (void *) 2;
    }
    memcpy(&seed_addr, &addr, sizeof(seed_addr));
    strncpy(the_host.addr, inet_ntoa(seed_addr.sin_addr), IP_LEN);
    //the_host.port = ntohs(seed_addr.sin_port);
    printf("source: %s:%d", inet_ntoa(seed_addr.sin_addr),
            ntohs(seed_addr.sin_port));
    close_socket(sock);
    return (void *) 0;
}

void * join_job_c(void * arg, int fd) {
    char msg[MSG_LEN];
    int sock;
    int j;
    s_host * host_buf;
    inplace_strip_tailing_slash(job.uri);
    snprintf(msg, MAX_URI, "JOIN\t%s\t%s\t%d", job.uri, the_host.addr,
            the_host.port);
    sock = connect_host(&the_server, 0, SND_TIMEOUT);
    if (sock < 0) {
        perr("connect_host error\n");
        return (void *) -1;
    }

    if ((j = sendall(sock, msg, MSG_LEN, 0)) < 0) {
        printf("sent: %d\n", j);
        perror("sending JOIN error!");
        return (void *) -1;
    }

    /*
     *  read and init some struct from serv
     */

    //read s_job
    j = recv(sock, (void *) (&job), sizeof(job), MSG_WAITALL);
    printf("job have read %d of %ld\n", j, sizeof(job));

    printf("job.file_count %lld\n", job.file_count);
    if (job.file_count) {
        //read s_file
        job.files = (s_file *) calloc(1, job.files_size);
        if (!job.files) {
            perr("job.files calloc failed\n");
        }
        j = recv(sock, job.files, job.files_size, MSG_WAITALL);
        printf("files have read %d of %lld\n", j, job.files_size);
        for (int i = 0; i < job.file_count; i++) {
            //printf("before: %s %s %s\n", (job.files + i)->name, job.uri, localpath);
            inplace_change_path((job.files + i)->name, job.uri, localpath);
            //printf("after: %s\n", (job.files + i)->name);
        }
    } else {
        //no file to process just quit
        return (void *) 0;
    }

    if (job.block_count) {
        //read s_block
        job.blocks = (s_block *) calloc(1, job.blocks_size);
        if (!job.blocks) {
            perr("job.blocks calloc failed\n");
        }
        j = recv(sock, job.blocks, job.blocks_size, MSG_WAITALL);
        printf("blocks have read %d of %lld\n", j, job.blocks_size);
    }
    //read hosts
    host_buf = (s_host *) calloc(job.host_num, sizeof(s_host));
    if (!host_buf) {
        perr("s_host buf calloc failed\n");
    }
    j = recv(sock, host_buf, job.host_num * sizeof(s_host), MSG_WAITALL);
    printf("hosts have read %d of %ld\n", j, job.host_num * sizeof(s_host));

    //put hosts into job.host_set
    job.host_set = new set<s_host> ;
    pthread_rwlock_wrlock(&clnt_lock);
    (*(job.host_set)).insert(host_buf, host_buf + job.host_num);
    pthread_rwlock_unlock(&clnt_lock);

    //copy(host_buf, host_buf+job.host_num, (*(job.host_set)).begin());
    // put the known host in the hash ring
    pthread_rwlock_rdlock(&clnt_lock);
    for (set<s_host>::iterator i = (*(job.host_set)).begin(); i
            != (*(job.host_set)).end(); i++) {
        printf("host in set:%s %d\n", i->addr, i->port);
        host_hash(&job, &(*i), NULL);
    }
    pthread_rwlock_unlock(&clnt_lock);
    //	if (job.file_count) {
    //		printf("host_num:%d, uri:%s, file1:%s, HOST:%s\n", job.host_num,
    //				job.uri, job.files->name, ((*(job.host_set)).begin())->addr);
    //	}
    printf("job.block_count: %lld\n", job.block_count);
    if (job.block_count) {
        pthread_rwlock_rdlock(&blk_hostset_lock);
        for (j = 0; j < job.block_count; j++) {
            if ((job.blocks + j)->host_set) {
                for (set<s_host>::iterator it =
                        (*((job.blocks + j)->host_set)).begin(); it
                        != (*((job.blocks + j)->host_set)).end(); it++) {
                    printf("%d %s:%d\n", j, it->addr, it->port);
                }
            }
        }
        pthread_rwlock_unlock(&blk_hostset_lock);
    }
    close_socket(sock);
    //init succeed :)
    return (void *) 0;
}

void * quit_job_s(void *, int fd) {
    printf("quit_job\n");
    return (void *) 0;
}

void * g_none_s(void *, int fd) {
    printf("none\n");
    return (void *) 0;
}

int async_server_base_init() {
    int port = MAX_LIS_PORT;
    server = (struct conn_server *) malloc(sizeof(struct conn_server));
    memset(server, 0, sizeof(struct conn_server));
    //server->srv_addr = inet_addr("0.0.0.0");
    server->srv_addr = seed_addr.sin_addr.s_addr;
    server->srv_port = port;
    server->listen_queue_length = REQ_QUE_LEN;
    server->nonblock = 1;
    server->tcp_nodelay = 1;
    server->tcp_reuse = 1;
    server->tcp_send_buffer_size = 131072;
    server->tcp_recv_buffer_size = 131072;
    server->send_timeout = SND_TIMEOUT;
    server->on_data_callback = conn_send_data;
    // A new TCP server
    while (conn_tcp_server(server) == -13) {
        if (port <= MIN_PORT) {
            perr("bind port failed ..wired!!!\n");
            exit(1);
        }
        server->srv_port = --port;
        usleep(10);
    }
    the_host.port = server->srv_port;
    return 0;
}

void * async_server(void * arg) {
    //    pthread_mutex_init(&mutex_join, NULL);
    // Event initialize
    ev_base = event_init();
    conn_client_list_init();
    thread_init();
    async_server_base_init();// haved event_base
    event_base_loop(ev_base, 0);
    //    pthread_mutex_destroy(&mutex_join);
    pthread_exit((void *) 0);
}

//we got ip from the previous "HELO" to the server
void *raw_server(void *arg) {
    char request[MAX_PACKET_LEN];
    int fd;
    short tmp_port;
    int l_sock, s_sock; /* master socket and slave socket */
    fd_set rfds, afds; /* ready fd set and active file set */
    socklen_t len;
    /* sin used to bind, sin_cli is the requesting client's sockaddr */
    struct sockaddr_in sin_cli;
    /* Fill up sin and allocate a passive socket */
    if ((l_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perr("Can't allocate a socket. %s\n", strerror(errno));
        exit(1);
    }

    tmp_port = MAX_PORT;
    seed_addr.sin_port = htons(tmp_port);
    while (bind(l_sock, (struct sockaddr *) &seed_addr, sizeof(seed_addr)) < 0) {
        if (tmp_port <= MIN_PORT) {
            perr("bind port failed ..wired!!!\n");
            exit(1);
        }
        seed_addr.sin_port = htons(tmp_port--);
        usleep(100);
    }
    if (listen(l_sock, REQ_QUE_LEN)) {
        perr("listen failed\n");
    }
    /* Some preparetions */FD_ZERO(&afds);
    FD_SET(l_sock, &afds);
    while (1) {
        memcpy((void *) &rfds, (void *) &afds, sizeof(afds));
        if (select(MAX_SELECT_FD, &rfds, NULL, NULL, (struct timeval *) 0) < 0) {
            perr("Select I/O Error %s\n", strerror(errno));
            exit(1);
        }
        if (FD_ISSET(l_sock, &rfds)) {
            len = sizeof(sin_cli);
            s_sock = accept(l_sock, (struct sockaddr *) &sin_cli, &len);
            if (s_sock < 0) {
                perr("Accept Error %s\n", strerror(errno));
                exit(1);
            }
            FD_SET(s_sock, &afds);
        }
        for (fd = 0; fd < MAX_SELECT_FD; fd++) {
            if ((fd != l_sock) && FD_ISSET(fd, &rfds)) {
                read(fd, request, MAX_PACKET_LEN);
                //free the s_session
                memset(request, 0, MAX_PACKET_LEN);
                close(fd);
                FD_CLR(fd, &afds);
            }
        }
    }
}

int mk_dir_softlink(void *) {
    s_file * tmp;
    for (int64_t i = 0; i < job.file_count; i++) {
        tmp = job.files + i;
        int fd = -1;
        printf("0%3o\t\t%lld\t\t\t%s\t%s\t", tmp->mode & 0777, tmp->size,
                (job.files + i)->name, tmp->sympath);
        switch (tmp->size) {
            case -1: // dir
                if (mkdir(tmp->name, tmp->mode)) {
                    perr("mkdir error: %s\n", strerror(errno));
                    return -1;
                }
                break;
            case -2: // symbol link
                if (symlink(tmp->sympath, tmp->name)) {
                    perr("symlink error: %s\n", strerror(errno));
                    return -1;
                }
                break;
            default: //regular file
                if (-1 == (fd = open(tmp->name, CREATE_OPEN_FLAG, tmp->mode))) {
                    perr("mk new file error: %s\n", strerror(errno));
                    return -1;
                } else {
                    close(fd);
                }

                break;
        }
        for (int j = 0; j < 16; j++)
            printf("%02x", (job.files + i)->md5[j]);
        printf("\n");

    }
    return 0;
}

int vnode_download(s_job * jo, int64_t blk_num, int64_t blk_count) {
    vector<s_host> h_vec;
    s_host h;
    int64_t i, tmp, blk_got = 0;
    char * buf;
    int retry = 0;
    /*
     * prepare the buf to recv data
     */
    if ((buf = (char *) malloc(BLOCK_SIZE)) == NULL) {
        perr("malloc for read buf of blocks_size failed");
        return -1;
    }
    while (blk_got < blk_count && retry <= MAX_RETRY) {
        i = get_blk_src(jo, 3, (blk_num + blk_got) % jo->block_count, &h_vec);
        printf("get_blk_src %lld\n", i);
        for (vector<s_host>::iterator iter = h_vec.begin(); iter != h_vec.end(); iter++) {
            printf("#####host in vec:%s %d\n", iter->addr, iter->port);
        }
        i = decide_src(jo, 3, (blk_num + blk_got) % jo->block_count,
                        &h_vec, &h);
        if (i <= 0) {
            retry++;
            perr("decide_src ret: %lld\n", i);
            sleep(retry);
        } else {
            retry = 0;
        }
        /*
         *  if intend to getblock from the_server
         *	just request one charge zone AND
         *	no more than MAX_REQ_SERV_BLOCKS
         */
        if (h == the_server) {
            //find the block zone edge
            int64_t count2edge = 1;
            // go back until we find the node has host
            pthread_rwlock_rdlock(&blk_hostset_lock);
            while (!((job.blocks + (blk_num + count2edge) % job.block_count)->host_set)
                    || (*((job.blocks + (blk_num + count2edge)
                            % job.block_count)->host_set)).size() == 0) {
                count2edge++;
            }
            pthread_rwlock_unlock(&blk_hostset_lock);
            tmp = getblock(&h, (blk_num + blk_got + i) % jo->block_count,
                    MIN(MAX_REQ_SERV_BLOCKS, count2edge), 0 | W_DISK, buf);
        } else {
            tmp = getblock(&h, (blk_num + blk_got + i) % jo->block_count,
                    blk_count - blk_got - i, 0 | W_DISK, buf);
        }
        if (tmp >= 0) {
            blk_got += tmp;
            blk_got += i;
        } else {
            perr("getblock ret: %lld\n", tmp);
        }
        printf("gotblock: %lld\n", tmp);
    }
    free(buf);
    if (retry > MAX_RETRY) {
        return -1;
    }
    return 0;
}

int node_download(void *) {
    if (!job.block_count) {
        return 0;
    }
    host_hash_result h_hash;
    //find out the initial block zone, save it in h_hash struct
    host_hash(&job, &the_host, &h_hash);
    //find incharge block count
    for (int i = 0; i < VNODE_NUM; i++) {
        int64_t j;
        j = h_hash.v_node[i];
        if (j == -1) {
            h_hash.length[i] = 0;
        }
        h_hash.length[i] = 1;
        // go back until we find the node has the_host
        while (!((job.blocks + (j + 1) % job.block_count)->host_set)
                || (*((job.blocks + (j + 1) % job.block_count)->host_set)).find(
                        the_host)
                        == (*((job.blocks + (j + 1) % job.block_count)->host_set)).end()) {

            //printf("j: %lld\n", j%job.block_count);
            h_hash.length[i]++;
            j++;
        }
        printf("vnode%d: %lld, length: %lld\n", i, h_hash.v_node[i],
                h_hash.length[i]);
    }
    for (int i = 0; i < VNODE_NUM; i++) {
        if (vnode_download(&job, h_hash.v_node[i], h_hash.length[i]) < 0) {
            exit(DOWNLOAD_ERR);
        }
    }
    return 0;
}

void * downloadworker(void *) {
    helo_serv_c(NULL, 0);
    //wait until the server thread listen a port

    while (the_host.port == 0) {
        usleep(100);
    }
    if (join_job_c((void *) job.uri, 0)) {
        perr("JOIN error %s\n", job.uri);
        exit(JOIN_ERR);
    }
    if (mk_dir_softlink(NULL)) {
        perr("mk_dir_softlink() failed\n");
        exit(MK_DIR_SYMLINK_ERR);
    }
    for (int64_t i = 0; i < job.block_count; i++) {
        (job.blocks + i)->done = 0;
    }
    //ready to serv !!
    ready_to_serv = 1;
    /*
     * insert and host_hash the hosts NEWWed before ready_to_serv
     */
    pthread_mutex_lock(&noready_mutex);
    pthread_rwlock_wrlock(&clnt_lock);
    (*(job.host_set)).insert(hosts_noready.begin(), hosts_noready.end());
    pthread_rwlock_unlock(&clnt_lock);

    for (vector<s_host>::iterator it = hosts_noready.begin(); it
            != hosts_noready.end(); it++) {
        host_hash(&job, &(*it), NULL);
    }
    pthread_mutex_unlock(&noready_mutex);
    if (node_download(NULL)) {
        perr("node_download failed\n");
        exit(DOWNLOAD_ERR);
    }
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@node_download done\n");
    for (int64_t i = 0; i < job.block_count; i++) {
        if ((job.blocks + i)->done != 1) {
            printf("undone: %lld\n", i);
        }
    }
    pthread_exit((void *) 0);
}

static inline int pthread_init() {
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_rwlock_init(&clnt_lock, NULL);
    pthread_rwlock_init(&blk_hostset_lock, NULL);
    pthread_mutex_init(&noready_mutex, NULL);
    pthread_mutex_init(&bw_up_mutex, NULL);
    pthread_mutex_init(&bw_down_mutex, NULL);
    return 0;
}

static inline int pthread_clean() {
    pthread_attr_destroy(&attr);
    pthread_rwlock_destroy(&clnt_lock);
    pthread_rwlock_destroy(&blk_hostset_lock);
    pthread_mutex_destroy(&noready_mutex);
    pthread_mutex_destroy(&bw_up_mutex);
    pthread_mutex_destroy(&bw_down_mutex);
    return 0;
}

int main(int argc, char *argv[]) {
    pthread_t download, upload;
    void *status;
    if (!(log_fp = fopen(CLIENT_LOG, "a+")))
        fprintf(stderr, "open log for appending failed, %s\n", strerror(errno));
#ifdef _BSD_MACHINE_TYPES_H_
    printf("DARWIN\n");
#else
    printf("LINUX\n");
#endif
    /* path test
     char path[MAX_PATH_LEN] = "../test/file";
     inplace_change_path(path, argv[1], argv[2]);
     printf("path: %s\n", path);
     */
    /* host_distance() test
     s_host h1 = {addr: "127.100.1.3", port: 0};
     s_host h2 = {addr: "127.100.0.3", port: 0};
     printf("distance: %lld\n", host_distance(&h1, &h2));
     */
    strncpy(the_server.addr, SERVER_IP, sizeof(the_server.addr));
    the_server.port = SERV_PORT;
    strncpy(job.uri, argv[1], sizeof(job.uri));
    strncpy(localpath, argv[2], sizeof(localpath));
    /*
     char path[MAX_PATH_LEN] = "../test/file";
     inplace_change_path(path, job.uri, job.path);
     printf("path: %s\n", path);
     */
    memset(&the_host, 0, sizeof(the_host));
    the_host.port = 0;
    serv = gethostbyname(SERVER_IP);
    if (!serv) {
        perr("gethostbyname error\n");
    }

    //downloadworker(NULL);
    pthread_init();
    pthread_create(&download, &attr, downloadworker, NULL);
    pthread_create(&upload, &attr, async_server, NULL);

    pthread_join(download, &status);
    sleep(60);
    return 0;
    pthread_join(upload, &status);
    pthread_clean();
    return 0;
}
