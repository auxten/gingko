/*
 *  gingko_base.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-10.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
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
#include <set>

#include "fnv_hash.h"
#include "gingko.h"
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /* __APPLE__ */

#include "event.h"

extern FILE * log_fp;

void perr(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char logstr[256] = "\0";
    gettimestr(logstr);
    vsnprintf(logstr + strlen(logstr), sizeof(logstr)-strlen(logstr), fmt, args);
    strerror_r(errno, logstr + strlen(logstr), sizeof(logstr)-strlen(logstr));
    fprintf(log_fp, "%s\n", logstr);
    fflush(log_fp);
    va_end(args);
    return;
}

void abort_handler(const int sig) {
    abort();
}

char * gettimestr(char * time){
    char * tm_format = "[%Y-%m-%d %H:%M:%S]  ";
    struct timeval tv;
    struct tm ltime;
    time_t curtime;
    gettimeofday(&tv, NULL);
    curtime=tv.tv_sec;
    //Format time
    strftime(time, 25, tm_format, localtime_r(&curtime, &ltime));
    return time;
}

int sep_arg(char * inputstring, char * arg_array[], int max) {
    char **ap;
    max++;
    for (ap = arg_array; (*ap = strsep(&inputstring, "\t")) != NULL;)
        if (**ap != '\0')
            if (++ap >= &arg_array[max])
                break;

    return 0;
}

extern char cmd_list[CMD_COUNT][8];
int parse_req(char *req) {
    u_int i;
    if (UNLIKELY(!req)) {
        return sizeof(cmd_list) / sizeof(cmd_list[0]) - 1;
    }
    for (i = 0; i < sizeof(cmd_list) / sizeof(cmd_list[0]) - 1; i++) {
        if (cmd_list[i][0] == req[0] && cmd_list[i][1] == req[1]
                && cmd_list[i][2] == req[2]) {
            break;
        }
    }
    return i;
}

/*
 * thread safe gethostbyname
 */
struct hostent * gethostname_my(const char *host, struct hostent *hostbuf,
        char ** tmphstbuf, size_t hstbuflen) {
#ifdef __APPLE__
    return gethostbyname(host);
#else
    struct hostent *hp;
    int res;
    int herr;

    while (UNLIKELY((res = gethostbyname_r(host, hostbuf, *tmphstbuf, hstbuflen,
                                   &hp, &herr)) == ERANGE)){
        /* Enlarge the buffer.  */
        hstbuflen *= 2;
        *tmphstbuf = (char *)realloc (*tmphstbuf, hstbuflen);
    }
    /*  Check for errors.  */
    if (res || hp == NULL)
      return NULL;
    return hp;
#endif
}

/*
 * connect to a host
 * h: pointer to s_host
 * recv_sec: receive timeout seconds, 0 for never timeout
 */
int connect_host(s_host * h, int recv_sec, int send_sec) {
    int sock;
    struct sockaddr_in channel;
    struct hostent *host;
    struct timeval recv_timeout, send_timeout;
    recv_timeout.tv_sec = recv_sec;
    recv_timeout.tv_usec = 0;
    send_timeout.tv_sec = send_sec;
    send_timeout.tv_usec = 0;
    char * gethostname_buf = (char *)malloc(1024);
    struct hostent host_buf;
    host = gethostname_my(h->addr, &host_buf, &gethostname_buf, 1024);
    if (FAIL_CHECK(!host)) {
        perr("gethostbyname %s error\n", h->addr);
        return -1;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0)) {
        perr("get socket error\n");
        return -1;
    }

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, host->h_addr, host->h_length);
    channel.sin_port = htons(h->port);
    //connect and send the msg
    if (FAIL_CHECK(0 > connect(sock, (struct sockaddr *) &channel, sizeof(channel)))) {
        free(gethostname_buf);
        perr("connect %s:%d ", h->addr, h->port);
        return -1;
    }
    free(gethostname_buf);
    if (recv_sec) {
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout,
                sizeof(struct timeval))) {
            perr("setsockopt SO_RCVTIMEO error");
            return -1;
        }
    }
    if (send_sec) {
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &send_timeout,
                sizeof(struct timeval))) {
            perr("setsockopt SO_SNDTIMEO error");
            return -1;
        }
    }
    return sock;
}

/*
 * gracefully close a socket, for client side
 */
int close_socket(int sock) {
    //	if (shutdown(sock, 2)) {
    //		perr("shutdown sock error %s\n", strerror(errno));
    //		return -1;
    //	}
    if (FAIL_CHECK(close(sock))) {
        perr("close sock error");
        return -1;
    }
    return 0;
}

/*
 * bwlimit modified from scp
 */
void bw_down_limit(int amount) {
    static struct timeval bw_down_start, bw_down_end;
    static int down_lamt, down_thresh = 16384;
    u_int64_t waitlen;
    struct timespec ts, rm;
    extern pthread_mutex_t bw_down_mutex;
    if (amount <= 0) {
        return;
    }
    pthread_mutex_lock(&bw_down_mutex);
    if (UNLIKELY(!timerisset(&bw_down_start))) {
        gettimeofday(&bw_down_start, NULL);
        pthread_mutex_unlock(&bw_down_mutex);
        return;
    }

    down_lamt += amount;
    if (down_lamt < down_thresh) {
        pthread_mutex_unlock(&bw_down_mutex);
        return;
    }

    gettimeofday(&bw_down_end, NULL);
    timersub(&bw_down_end, &bw_down_start, &bw_down_end);
    if (!timerisset(&bw_down_end)) {
        pthread_mutex_unlock(&bw_down_mutex);
        return;
    }

    waitlen = (double) 1000000L * down_lamt / LIMIT_DOWN_RATE;

    bw_down_start.tv_sec = waitlen / 1000000L;
    bw_down_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&bw_down_start, &bw_down_end, >)) {
        timersub(&bw_down_start, &bw_down_end, &bw_down_end);

        /* Adjust the wait time */
        if (bw_down_end.tv_sec) {
            down_thresh /= 2;
            if (down_thresh < 2048)
                down_thresh = 2048;
        } else if (bw_down_end.tv_usec < 100) {
            down_thresh *= 2;
            if (down_thresh > 32768)
                down_thresh = 32768;
        }

        TIMEVAL_TO_TIMESPEC(&bw_down_end, &ts);
        //fprintf(stderr, "sleep for: %d usec\n", (&bw_down_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1) {
            if (errno != EINTR)
                break;
            ts = rm;
        }
    }

    down_lamt = 0;
    gettimeofday(&bw_down_start, NULL);
    pthread_mutex_unlock(&bw_down_mutex);
    return;
}

void bw_up_limit(int amount) {
    static struct timeval bw_up_start, bw_up_end;
    static int up_lamt, up_thresh = 16384;
    u_int64_t waitlen;
    struct timespec ts, rm;
    extern pthread_mutex_t bw_up_mutex;
    if (amount <= 0) {
        return;
    }
    pthread_mutex_lock(&bw_up_mutex);
    if (UNLIKELY(!timerisset(&bw_up_start))) {
        gettimeofday(&bw_up_start, NULL);
        pthread_mutex_unlock(&bw_up_mutex);
        return;
    }

    up_lamt += amount;
    if (up_lamt < up_thresh) {
        pthread_mutex_unlock(&bw_up_mutex);
        return;
    }

    gettimeofday(&bw_up_end, NULL);
    timersub(&bw_up_end, &bw_up_start, &bw_up_end);
    if (!timerisset(&bw_up_end)) {
        pthread_mutex_unlock(&bw_up_mutex);
        return;
    }

    waitlen = (double) 1000000L * up_lamt / LIMIT_UP_RATE;

    bw_up_start.tv_sec = waitlen / 1000000L;
    bw_up_start.tv_usec = waitlen % 1000000L;

    if (timercmp(&bw_up_start, &bw_up_end, >)) {
        timersub(&bw_up_start, &bw_up_end, &bw_up_end);

        /* Adjust the wait time */
        if (bw_up_end.tv_sec) {
            up_thresh /= 2;
            if (up_thresh < 2048)
                up_thresh = 2048;
        } else if (bw_up_end.tv_usec < 100) {
            up_thresh *= 2;
            if (up_thresh > 32768)
                up_thresh = 32768;
        }

        TIMEVAL_TO_TIMESPEC(&bw_up_end, &ts);
        //fprintf(stderr, "sleep for: %d usec\n", (&bw_up_end)->tv_usec);
        while (nanosleep(&ts, &rm) == -1) {
            if (errno != EINTR)
                break;
            ts = rm;
        }
    }

    up_lamt = 0;
    gettimeofday(&bw_up_start, NULL);
    pthread_mutex_unlock(&bw_up_mutex);
    return;
}

/*
 * Before send data all req to server will filtered by conn_send_data
 */
extern func_t func_list_s[CMD_COUNT];
void conn_send_data(int fd, void *str, unsigned int len) {
    int i;
    char * p = (char *) str;
    printf("got req: %s\n", p);
    i = parse_req(p);
    (func_list_s[i])(p, fd);
    //printf("read_buffer:%s\n", p);
    return;
}

/*
 * event handle of write
 */
void ev_fn_write(int fd, short ev, void *arg) {
    s_write_arg * a = (s_write_arg *) arg;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 1;
    if (((a->sent = send(fd, a->p + a->send_counter, a->sz - a->send_counter,
            a->flag)) >= 0) || ERR_RW_RETRIABLE(errno)) {
        //printf("res: %d\n",res);
        //printf("%s",client->read_buffer+read_counter);
        //printf("sent: %d\n", a->sent);
        if ((a->send_counter = MAX(a->sent, 0) + a->send_counter) == a->sz) {
            event_del(&(a->ev_write));
        }
        a->retry = 0;
    } else {
        if(a->retry++ > 3) {
            event_base_loopexit(a->ev_base, &t);
        }
    }
    return;
}

/*
 * send a mem to fd(usually socket)
 */
int sendall(int fd, const void * void_p, int sz, int flag) {
    s_write_arg arg;
    if (!sz) {
        return 0;
    }
    if (!void_p) {
        perror("Null Pointer\n");
        return -2;
    }
    arg.sent = 0;
    arg.send_counter = 0;
    arg.sz = sz;
    arg.p = (char *) void_p;
    arg.flag = flag;
    arg.retry = 0;
    // FIXME event_init() and event_base_free() waste open pipe
    arg.ev_base = event_init();

    event_set(&(arg.ev_write), fd, EV_WRITE | EV_PERSIST, ev_fn_write,
            (void *) (&arg));
    event_base_set(arg.ev_base, &(arg.ev_write));
    if (-1 == event_add(&(arg.ev_write), 0)) {
        fprintf(stderr, "Cannot handle write data event\n");
    }
    event_base_loop(arg.ev_base, 0);
    event_del(&(arg.ev_write));
    event_base_free(arg.ev_base);
    //printf("sent: %d", arg.sent);
    if (arg.sent < 0) {
        printf("errno: %d", errno);
        perror("ev_fn_write error");
        return -1;
    }
    return 0;
}

/*
 * send conut bytes file  from in_fd to out_fd at offset
 */
int sendfileall(int out_fd, int in_fd, off_t *offset, u_int64_t *count) {
    s_gsendfile_arg arg;
    if (!*count) {
        return 0;
    }
    arg.sent = 0;
    arg.send_counter = 0;
    arg.in_fd = in_fd;
    arg.offset = *offset;
    arg.count = *count;
    arg.retry = 0;
    // FIXME event_init() and event_base_free() waste open pipe
    arg.ev_base = event_init();

    event_set(&(arg.ev_write), out_fd, EV_WRITE | EV_PERSIST, ev_fn_gsendfile,
            (void *) (&arg));
    event_base_set(arg.ev_base, &(arg.ev_write));
    if (-1 == event_add(&(arg.ev_write), 0)) {
        fprintf(stderr, "Cannot handle write data event\n");
    }
    event_base_loop(arg.ev_base, 0);
    event_del(&(arg.ev_write));
    event_base_free(arg.ev_base);
    if (arg.sent < 0) {
        printf("errno: %d", errno);
        perror("ev_fn_gsendfile error");
        return -1;
    }
    return 0;
}

/*
 * check if the fnv check sum is OK?
 */
int digest_ok(void * buf, s_block * b) {
    return (fnv_hash(buf, b->size, 0) == b->digest);
}

/*
 * send blocks to the out_fd(usually socket)
 */
int sendblocks(int out_fd, s_job * jo, int64_t start, int64_t num) {
    if (num <= 0) {
        return 0;
    }
    /*
     * if include the last block, the total size
     *    may be smaller than BLOCK_SIZE * num
     */
    int64_t sent = 0;
    int64_t total = BLOCK_SIZE * (num - 1)
            + (start + num >= jo->block_count ? (jo->blocks + jo->block_count
                    - 1)->size : BLOCK_SIZE);
    int64_t b = start;
    int64_t f = (jo->blocks + start)->start_f;
    u_int64_t block_left = (jo->blocks + b)->size;
    int64_t file_size;
    int fd = -1;
    off_t offset = (jo->blocks + start)->start_off;
    u_int64_t file_left;
    while (total > 0) {
        if (fd == -1) {
            fd = open((jo->files + f)->name, READ_OPEN_FLAG);
        }
        if (fd < 0) {
            perror("sendblocks open error");
            return -2;
        }
        file_size = (jo->files + f)->size;
        file_left = file_size - offset;

        if (block_left > file_left) {
            if (sendfileall(out_fd, fd, &offset, &file_left) != 0) {
                perror("sendfileall error");
                close(fd);
                return -1;
            }
            close(fd);
            fd = -1;
            block_left -= file_left;
            total -= file_left;
            sent += file_left;
            offset = 0;
            f = next_f(jo, f);
        } else if (block_left == file_left) {
            if (sendfileall(out_fd, fd, &offset, &file_left) != 0) {
                perror("sendfileall error");
                close(fd);
                return -1;
            }
            close(fd);
            fd = -1;
            offset = 0;
            total -= file_left;
            sent += file_left;
            f = next_f(jo, f);
            b = next_b(jo, b);
            block_left = (jo->blocks + b)->size;
        } else { /*block_left < file_left*/
            if (sendfileall(out_fd, fd, &offset, &block_left) != 0) {
                perror("sendfileall error");
                close(fd);
                return -1;
            }
            b = next_b(jo, b);
            total -= block_left;
            offset += block_left;
            sent += block_left;
            block_left = (jo->blocks + b)->size;
        }
        //fprintf(stderr, "block %ld \n", sent/BLOCK_SIZE);
    }
    return 0;
}

/*
 * write block to disk
 */
int writeblock(s_job * jo, const unsigned char * buf, s_block * blk) {
    int64_t f = blk->start_f;
    off_t offset = blk->start_off;
    int64_t total = blk->size;
    int64_t counter = 0;
    int64_t file_size;

    while (total > 0) {
        int64_t wrote;
        s_file * file = jo->files + f;
        file_size = file->size;
        int fd = open(file->name, WRITE_OPEN_FLAG);
        if (fd < 0) {
            fprintf(stderr, "%s:", file->name);
            perror("open file for write error");
            return -2;
        }
        wrote = pwrite(fd, buf + counter, MIN(file_size - offset, total),
                offset);
        if (wrote < 0) {
            perror("write file error");
            return -1;
        }
        fsync(fd);
        total -= wrote;
        counter += wrote;
        close(fd);
        f = next_f(jo, f);
        offset = 0;
    }
    return counter;
}

/*
 * send cmd msg to host, not read response
 */
int sendcmd(s_host *h, const char * cmd, int recv_sec, int send_sec) {
    int sock, result;
    sock = connect_host(h, recv_sec, send_sec);
    if (sock < 0) {
        perr("sendcmd() connect_host error");
        return -1;
    }
    result = sendall(sock, cmd, strlen(cmd), 0);
    close_socket(sock);
    return result;
}
