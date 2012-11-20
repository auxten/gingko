/**
 *  gingko_base.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-10.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/

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
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#ifdef __APPLE__
#include <sys/uio.h>
#elif defined (__FreeBSD__)
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <sys/sendfile.h>
#endif /** __APPLE__ **/
#ifdef __APPLE__
#include <poll.h>
#else
#include <sys/poll.h>
#endif /** __APPLE__ **/

#include "event.h"
#include "config.h"

#include "hash/xor_hash.h"
#include "hash/gko_zip.h"
#include "gingko.h"
#include "path.h"
#include "log.h"
#include "socket.h"
#include "limit.h"
#include "async_pool.h"

///mutex for gethostbyname limit
static pthread_mutex_t g_netdb_mutex = PTHREAD_MUTEX_INITIALIZER;


/// global struct gko
extern s_gingko_global_t gko;

/**
 * @brief Set signal handler
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void set_sig(void(*int_handler)(int))
{
    struct sigaction sa;

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    signal(SIGQUIT, int_handler);
    signal(SIGHUP, SIG_IGN);

    /** Ignore terminal signals **/
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    /** Ignore SIGPIPE & SIGCLD **/
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    return;
}

/**
 * @brief start a thread to watch the gko.sig_flag and take action
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sig_watcher(void * (*worker)(void *))
{
    pthread_attr_t attr;
    pthread_t sig_watcher_thread;

    /** initialize the pthread attrib for every worker **/
    if (pthread_attr_init(&attr) != 0)
    {
        gko_log(FATAL, FLF("pthread_attr_init error"));
        return -1;
    }
    /** set attr state for join() in the mother **/
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
    {
        gko_log(FATAL, FLF("pthread_attr_setdetachstate error"));
        return -1;
    }
    if (pthread_attr_setstacksize(&attr, MYSTACKSIZE) != 0)
    {
        gko_log(FATAL, FLF("pthread_attr_setstacksize error"));
        return -1;
    }

    if (pthread_create(&sig_watcher_thread, &attr, worker, NULL) != 0)
    {
        gko_log(FATAL, FLF("pthread_create error"));
        return -1;
    }

    return 0;
}

/**
 * @brief separate the element from a req string
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sep_arg(char * inputstring, char * arg_array[], int max)
{
    char **ap;
    int i = 0;
    max++;
    for (ap = arg_array; (*ap = strsep(&inputstring, "\t")) != NULL;)
    {
        if (**ap != '\0')
        {
            if (i++, ++ap >= &arg_array[max])
            {
                break;
            }
        }
    }

    return i;
}

/**
 * @brief thread safe gethostbyname
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
struct hostent * gethostname_my(const char *host, struct hostent * ret)
{
    struct hostent * tmp;
    if (!ret)
    {
        gko_log(FATAL, "Null buf passed to gethostname_my error");
        return (struct hostent *) NULL;
    }

    pthread_mutex_lock(&g_netdb_mutex);
    tmp = gethostbyname(host);
    if (tmp)
    {
        memcpy(ret, tmp, sizeof(struct hostent));
    }
    else
    {
        gko_log(WARNING, "resolve %s failed", host);
        ret = NULL;
    }
    pthread_mutex_unlock(&g_netdb_mutex);

    return ret;
}

/**
 * @brief get host in_addr_t, this only works on IPv4. that is enough
 *        return h_length, on error return 0
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-25
 **/
int getaddr_my(const char *host, in_addr_t * addr_p)
{
    struct hostent * tmp;
    int ret = 0;
    if (!addr_p)
    {
        gko_log(FATAL, "Null buf passed to getaddr_my error");
        return 0;
    }

    pthread_mutex_lock(&g_netdb_mutex);
    tmp = gethostbyname(host);
    if (tmp)
    {
        *addr_p = *(in_addr_t *)tmp->h_addr;
        ret = tmp->h_length;
    }
    else
    {
        gko_log(WARNING, "resolve %s failed", host);
        ret = 0;
    }
    pthread_mutex_unlock(&g_netdb_mutex);

    return ret;
}

/**
 * @brief check the ulimit -n
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-24
 **/
int check_ulimit()
{
    struct rlimit lmt;
    if(getrlimit(RLIMIT_NOFILE, &lmt) != 0)
    {
        gko_log(FATAL, "getrlimit(RLIMIT_NOFILE, &lmt) error");
        return -1;
    }
    if (lmt.rlim_max < MIN_NOFILE)
    {
        fprintf(
                stderr,
                "The max open files limit is %lld, you had better make it >= %lld\n",
                lmt.rlim_max, MIN_NOFILE);
        gko_log(
                FATAL,
                "The max open files limit is %lld, you had better make it >= %lld\n",
                lmt.rlim_max, MIN_NOFILE);
        return -1;
    }
    return 0;
}

/**
 * @brief event handle of write
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void ev_fn_gsend(int fd, short ev, void *arg)
{
    s_write_arg_t * a = (s_write_arg_t *) arg;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 1;
    if (((a->sent = send(fd, a->p + a->send_counter, a->sz - a->send_counter,
            a->flag)) >= 0) || ERR_RW_RETRIABLE(errno))
    {
        gko_log(DEBUG, "ev_fn_gsend sent: %d", a->sent);
        if ((a->send_counter = MAX(a->sent, 0) + a->send_counter) == a->sz)
        {
            event_del(&(a->ev_write));
        }
        a->retry = 0;
    }
    else
    {
        gko_log(WARNING, "gsend error");
        if ((a->retry)++ > 3)
        {
            event_del(&(a->ev_write));
            event_base_loopexit(a->ev_base, &t);
        }
    }
    return;
}


/**
 * @brief send a mem to fd(usually socket)
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendall(int fd, const void * void_p, int sz, int timeout)
{
    int sent = 0;
    int send_counter = 0;
    int retry = 0;
    int select_ret;
    char * p;
    struct timeval send_timeout;
    send_timeout.tv_sec = timeout;
    send_timeout.tv_usec = 0;


    if (!sz)
    {
        return 0;
    }
    if (!void_p)
    {
        gko_log(WARNING, "Null Pointer");
        return -1;
    }
    p =(char *)void_p;

    while (send_counter < sz)
    {
#if HAVE_POLL
        {
            struct pollfd pollfd;

            pollfd.fd = fd;
            pollfd.events = POLLOUT;

            /* send_sec is in seconds, timeout in ms */
            select_ret = poll(&pollfd, 1, (int)(timeout * 1000 + 1));
        }
#else
        {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(fd, &wset);
            select_ret = select(fd + 1, 0, &wset, 0, &send_timeout);
        }
#endif /* HAVE_POLL */
        if (select_ret < 0)
        {
            if (ERR_RW_RETRIABLE(errno))
            {
                if (retry++ > 3)
                {
                    gko_log(WARNING, "select error over 3 times");
                    return -1;
                }
                continue;
            }
            gko_log(WARNING, "select/poll error on sendall");
            return -1;
        }
        else if (!select_ret)
        {
            gko_log(NOTICE, "select/poll timeout on sendall");
            return -1;
        }
        else
        { /** select/poll returned a fd **/
            if (((sent = send(fd, p + send_counter, sz - send_counter,
                    0)) >= 0) || ERR_RW_RETRIABLE(errno))
            {
                gko_log(DEBUG, "sendall sent: %d", sent);
                retry = 0;
                send_counter += MAX(sent, 0);
            }
            else
            {
                if (retry++ > 3)
                {
                    gko_log(DEBUG, "send error over 3 times");
                    return -1;
                }
            }

        }
    }

    return send_counter;
}

/**
 * @brief event handle of sendfile
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void ev_fn_gsendfile(int fd, short ev, void *arg)
{
    s_gsendfile_arg_t * a = (s_gsendfile_arg_t *) arg;
    off_t tmp_off = a->offset + a->send_counter;
    GKO_UINT64 tmp_counter = a->count - a->send_counter;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 1;
    if (((a->sent = gsendfile(fd, a->in_fd, &tmp_off, &tmp_counter)) >= 0))
    {
        bw_up_limit(a->sent, gko.opt.limit_up_rate);
//        gko_log(DEBUG, "gsentfile: %d", a->sent);
        if ((a->send_counter += a->sent) == a->count)
        {
            event_del(&(a->ev_write));
        }
        a->retry = 0;
    }
    else
    {
        if (a->retry++ > 3)
        {
            event_del(&(a->ev_write));
            event_base_loopexit(a->ev_base, &t);
        }
    }
    return;
}

/**
 * @brief send conut bytes file  from in_fd to out_fd at offset
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendfileall(int out_fd, int in_fd, off_t *offset, GKO_UINT64 *count)
{
    s_gsendfile_arg_t arg;
    if (!*count)
    {
        return 0;
    }
    arg.sent = 0;
    arg.send_counter = 0;
    arg.in_fd = in_fd;
    arg.offset = *offset;
    arg.count = *count;
    arg.retry = 0;
    /// FIXME event_init() and event_base_free() waste open pipe
    arg.ev_base = (struct event_base*)event_init();

    event_set(&(arg.ev_write), out_fd, EV_WRITE | EV_PERSIST, ev_fn_gsendfile,
            (void *) (&arg));
    event_base_set(arg.ev_base, &(arg.ev_write));
    if (-1 == event_add(&(arg.ev_write), 0))
    {
        gko_log(WARNING, "Cannot handle write data event");
    }
    event_base_loop(arg.ev_base, 0);
    event_del(&(arg.ev_write));
    event_base_free(arg.ev_base);
    if (arg.sent < 0)
    {
        gko_log(DEBUG, "ev_fn_gsendfile error %lld", arg.sent);
        return -1;
    }
    return 0;
}

/**
 * @brief send conut bytes file  from in_fd to out_fd at offset
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendfileall2(int out_fd, int in_fd, off_t *offset, GKO_UINT64 *count)
{
    s_gsendfile_arg_t arg;
    if (!*count)
    {
        return 0;
    }
    arg.sent = 0;
    arg.send_counter = 0;
    arg.in_fd = in_fd;
    arg.offset = *offset;
    arg.count = *count;
    arg.retry = 0;
    /// FIXME event_init() and event_base_free() waste open pipe
    arg.ev_base = (struct event_base*)event_init();

    event_set(&(arg.ev_write), out_fd, EV_WRITE | EV_PERSIST, ev_fn_gsendfile,
            (void *) (&arg));
    event_base_set(arg.ev_base, &(arg.ev_write));
    if (-1 == event_add(&(arg.ev_write), 0))
    {
        gko_log(WARNING, "Cannot handle write data event");
    }
    event_base_loop(arg.ev_base, 0);
    event_del(&(arg.ev_write));
    event_base_free(arg.ev_base);
    if (arg.sent < 0)
    {
        gko_log(DEBUG, "ev_fn_gsendfile error %lld", arg.sent);
        return -1;
    }
    return 0;
}

/**
 * @brief zip the buf, fill cmd header and send it out
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date Jan 12, 2012
 **/
int zip_sendall(int out_fd, char * buf_orig, char * buf_zip, int orig_len)
{

    int zip_len = gko_zip(buf_orig, buf_zip+CMD_PREFIX_BYTE, orig_len);
    if (zip_len <= 0)
    {
        gko_log(WARNING, FLF("compress error"));
        return -1;
    }
    gko_log(DEBUG, "zip len %d", zip_len);

    fill_cmd_head(buf_zip, zip_len + CMD_PREFIX_BYTE);
    if (sendall(out_fd, buf_zip, zip_len + CMD_PREFIX_BYTE, SNDBLK_TIMEOUT) < 0)
    {
        gko_log(WARNING, FLF("sendall error"));
        return -2;
    }
    bw_up_limit(zip_len + CMD_PREFIX_BYTE, gko.opt.limit_up_rate);

    return zip_len + CMD_PREFIX_BYTE;
}

/**
 * @brief try best to read specificed bytes from a file to buf
 * @brief so the count should not be very large...
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int readfileall(int fd, off_t offset, off_t count, char ** buf)
{
    int res = 0;
    off_t read_counter = 0;
    if (!count)
    {
        return 0;
    }

    /// Initialize buffer
    *buf = new char[count];

    while (read_counter < count && (res = pread(fd, *buf + read_counter,
            count - read_counter, offset + read_counter)) > 0)
    {
        gko_log(DEBUG, "readfileall res: %d",res);
        read_counter += res;
    }
    if (res < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            gko_log(FATAL, "File read error");
            delete [] (*buf);
            return -1;
        }
    }
    return 0;
}


/**
 * @brief try best to read specificed bytes from a file to buf
 * @brief so the count should not be very large...
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int readfileall_append(int fd, off_t offset, off_t count, char * buf)
{
    int res = 0;
    off_t read_counter = 0;
    if (!count)
    {
        return 0;
    }

    while (read_counter < count && (res = pread(fd, buf + read_counter,
            count - read_counter, offset + read_counter)) > 0)
    {
        gko_log(DEBUG, "readfileall_append res: %d",res);
        read_counter += res;
    }
    if (res < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            gko_log(FATAL, "File read error");
            return -1;
        }
    }
    disk_r_limit(read_counter, gko.opt.limit_disk_r_rate);
    return 0;
}


/**
 * @brief read all from a socket by best effort
 *
 * @see
 * @note
 * receive all the data or returns error (handles EINTR etc.)
 * params: socket
 *         data     - buffer for the results
 *         data_len -
 *         flags    - recv flags for the first recv (see recv(2)), only
 *                    0, MSG_WAITALL and MSG_DONTWAIT make sense
 * if flags is set to MSG_DONWAIT (or to 0 and the socket fd is non-blocking),
 * and if no data is queued on the fd, recv_all will not wait (it will
 * return error and set errno to EAGAIN/EWOULDBLOCK). However if even 1 byte
 *  is queued, the call will block until the whole data_len was read or an
 *  error or eof occured ("semi-nonblocking" behaviour,  some tcp code
 *   counts on it).
 * if flags is set to MSG_WAITALL it will block even if no byte is available.
 *
 * returns: bytes read or error (<0)
 * can return < data_len if EOF
 *
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int readall(int fd, void* data, int data_len, int timeout)
{
    int b_read = 0;
    int n = 0;
    int retry = 0;
    int select_ret = 0;
    struct timeval read_timeout;
    read_timeout.tv_sec = timeout;
    read_timeout.tv_usec = 0;

    while (b_read < data_len)
    {
#if HAVE_POLL
        {
            struct pollfd pollfd;

            pollfd.fd = fd;
            pollfd.events = POLLIN;

            /* send_sec is in seconds, timeout in ms */
            select_ret = poll(&pollfd, 1, (int)(timeout * 1000 + 1));
        }
#else
        {
            fd_set wset;
            FD_ZERO(&wset);
            FD_SET(fd, &wset);
            select_ret = select(fd + 1, &wset, 0, 0, &read_timeout);
        }
#endif /* HAVE_POLL */
        if (select_ret < 0)
        {
            if (ERR_RW_RETRIABLE(errno))
            {
                if (retry++ > 3)
                {
                    gko_log(WARNING, "select error over 3 times");
                    return -1;
                }
                continue;
            }
            gko_log(WARNING, "select/poll error on readall");
            return -1;
        }
        else if (!select_ret)
        {
            gko_log(NOTICE, "select/poll timeout on readall");
            return -1;
        }
        else
        { /** select/poll returned a fd **/
            n = recv(fd, (char*) data + b_read, data_len - b_read, 0);
            if (UNLIKELY((n < 0 && !ERR_RW_RETRIABLE(errno)) || !n))
            {
                gko_log(WARNING, "readall error");
                return -1;
            }
            else
            {
                if (n < 0)
                {
                    if (retry++ > 3)
                    {
                        gko_log(WARNING, "readall timeout");
                        return -1;
                    }
                }
                else
                {
                    b_read += n;
                    if (data_len > READ_LIMIT_THRESHOLD)
                    {
                        bw_down_limit(n, gko.opt.limit_down_rate);
                    }
                }
            }

        }
    }//while

    return b_read;
}

/**
 * @brief read cmd, first 2 bytes are the length
 *
 * @see
 * @note
 *
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int readcmd(int fd, void* data, int max_len, int timeout)
{
    unsigned short proto_ver;
    int msg_len;
    int ret;
    char nouse_buf[CMD_PREFIX_BYTE];

    /// read the proto ver
    if (readall(fd, &proto_ver, sizeof(proto_ver), timeout) < 0)
    {
        gko_log(WARNING, "read proto_ver failed");
        return -1;
    }
    if (proto_ver != PROTO_VER)
    {
        gko_log(WARNING, "unsupported proto_ver");
        return -1;
    }

    /// read the nouse_buf
    if (readall(fd, nouse_buf,
            CMD_PREFIX_BYTE - sizeof(proto_ver) - sizeof(msg_len), timeout) < 0)
    {
        gko_log(WARNING, "read nouse_buf failed");
        return -1;
    }

    /// read the msg_len
    if (readall(fd, &msg_len, sizeof(msg_len), timeout) < 0)
    {
        gko_log(WARNING, "read msg_len failed");
        return -1;
    }
    if (msg_len > max_len)
    {
        gko_log(WARNING, "a too long msg: %u", msg_len);
        return -1;
    }
    ret = readall(fd, data, msg_len, timeout);
    if (ret < 0)
    {
        gko_log(WARNING, "read cmd failed");
        return -1;
    }
    return ret;
}

/**
 * @brief read zipped data form fd and unzip it
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date Jan 12, 2012
 **/
int readall_unzip(int fd, char * data, char * buf_zip, int data_len, int timeout)
{
    if (readcmd(fd, buf_zip, data_len + data_len * 6 / 1024 + UNZIP_EXTRA, timeout) < 0)
    {
        gko_log(NOTICE, FLF("readall_unzip error"));
        return -1;
    }
    if (gko_unzip(buf_zip, data, data_len) < 0)
    {
        gko_log(WARNING, FLF("gko_unzip error"));
        return -2;
    }
    return data_len;
}


/**
 * @brief send blocks to the out_fd(usually socket)
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendblocks(int out_fd, s_job_t * jo, GKO_INT64 start, GKO_INT64 num)
{
    if (num <= 0)
    {
        return 0;
    }
    /**
     * if include the last block, the total size
     *    may be smaller than BLOCK_SIZE * num
     **/
    GKO_INT64 sent = 0;
    GKO_INT64 b = start;
    GKO_INT64 file_size;
    GKO_UINT64 file_left;
    int fd = -1;

    GKO_INT64 total = BLOCK_SIZE * (num - 1)
            + (start + num >= jo->block_count ? (jo->blocks + jo->block_count
                    - 1)->size : BLOCK_SIZE);
    GKO_INT64 f = (jo->blocks + start)->start_f;
    GKO_UINT64 block_left = (jo->blocks + b)->size;
    off_t offset = (jo->blocks + start)->start_off;
    while (total > 0)
    {
        if (fd == -1)
        {
            fd = open((jo->files + f)->name, READ_OPEN_FLAG);
        }
        if (fd < 0)
        {
            gko_log(WARNING, "sendblocks open error");
            return -2;
        }
        file_size = (jo->files + f)->size;
        file_left = file_size - offset;

        if (block_left > file_left)
        {
            if (sendfileall(out_fd, fd, &offset, &file_left) != 0)
            {
                gko_log(DEBUG, FLF("sendfileall error"));
                close(fd);
                fd = -1;
                return -1;
            }
            close(fd);
            fd = -1;
            block_left -= file_left;
            total -= file_left;
            sent += file_left;
            offset = 0;
            f = next_f(jo, f);
        }
        else if (block_left == file_left)
        {
            if (sendfileall(out_fd, fd, &offset, &file_left) != 0)
            {
                gko_log(DEBUG, FLF("sendfileall error"));
                close(fd);
                fd = -1;
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
        }
        else
        { /**block_left < file_left**/
            if (sendfileall(out_fd, fd, &offset, &block_left) != 0)
            {
                gko_log(DEBUG, FLF("sendfileall error"));
                close(fd);
                fd = -1;
                return -1;
            }
            b = next_b(jo, b);
            total -= block_left;
            offset += block_left;
            sent += block_left;
            block_left = (jo->blocks + b)->size;
        }
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
    }
    return 0;
}

/**
 * @brief send zipped blocks to the out_fd(usually socket)
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendblocks_zip(int out_fd, s_job_t * jo, GKO_INT64 start, GKO_INT64 num)
{
    if (num <= 0)
    {
        return 0;
    }
    GKO_INT64 sent = 0;
    GKO_INT64 b = start;
    GKO_INT64 file_size;
    GKO_UINT64 file_left;
    int fd = -1;
    int ret = 0;
    char * buf_orig = new char [BLOCK_SIZE + 1];
    char * buf_zip = new char [BLOCK_SIZE + BLOCK_SIZE * 6 / 1024 + CMD_PREFIX_BYTE];

    /**
     * if include the last block, the total size
     *    may be smaller than BLOCK_SIZE * num
     **/
    GKO_INT64 total = BLOCK_SIZE * (num - 1)
            + (start + num >= jo->block_count ? (jo->blocks + jo->block_count
                    - 1)->size : BLOCK_SIZE);
    GKO_INT64 f = (jo->blocks + start)->start_f;
    GKO_UINT64 block_left = (jo->blocks + b)->size;
    off_t offset = (jo->blocks + start)->start_off;
    while (total > 0)
    {
        if (fd == -1)
        {
            fd = open((jo->files + f)->name, READ_OPEN_FLAG);
        }
        if (fd < 0)
        {
            gko_log(WARNING, FLF("sendblocks_zip open error"));
            ret = -2;
            goto sendblocks_zip_exit;
        }
        file_size = (jo->files + f)->size;
        file_left = file_size - offset;

        if (block_left > file_left)
        {
            if (readfileall_append(fd, offset, file_left, buf_orig + (jo->blocks + b)->size - block_left) != 0)
            ///if (sendfileall(out_fd, fd, &offset, &file_left) != 0)
            {
                gko_log(DEBUG, FLF("readfileall error"));
                close(fd);
                fd = -1;
                ret = -1;
                goto sendblocks_zip_exit;
            }
            close(fd);
            fd = -1;
            block_left -= file_left;
            total -= file_left;
            sent += file_left;
            offset = 0;
            f = next_f(jo, f);
        }
        else if (block_left == file_left)
        {
            if (readfileall_append(fd, offset, file_left, buf_orig + (jo->blocks + b)->size - block_left) != 0)
            ///if (sendfileall(out_fd, fd, &offset, &file_left) != 0)
            {
                gko_log(DEBUG, FLF("readfileall error"));
                close(fd);
                fd = -1;
                ret = -1;
                goto sendblocks_zip_exit;
            }

            if (zip_sendall(out_fd, buf_orig, buf_zip, (jo->blocks + b)->size) < 0)
            {
                gko_log(WARNING, FLF("sendall error"));
                close(fd);
                fd = -1;
                ret = -1;
                goto sendblocks_zip_exit;
            }

            close(fd);
            fd = -1;
            offset = 0;
            total -= file_left;
            sent += file_left;
            f = next_f(jo, f);
            b = next_b(jo, b);
            block_left = (jo->blocks + b)->size;
        }
        else
        { /**block_left < file_left**/
            if (readfileall_append(fd, offset, block_left, buf_orig + (jo->blocks + b)->size - block_left) != 0)
            ///if (sendfileall(out_fd, fd, &offset, &block_left) != 0)
            {
                gko_log(DEBUG, FLF("readfileall error"));
                close(fd);
                fd = -1;
                ret = -1;
                goto sendblocks_zip_exit;
            }

            if (zip_sendall(out_fd, buf_orig, buf_zip, (jo->blocks + b)->size) < 0)
            {
                gko_log(WARNING, FLF("sendall error"));
                close(fd);
                fd = -1;
                ret = -1;
                goto sendblocks_zip_exit;
            }

            b = next_b(jo, b);
            total -= block_left;
            offset += block_left;
            sent += block_left;
            block_left = (jo->blocks + b)->size;
        }
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
    }

sendblocks_zip_exit:
    delete [] buf_orig;
    delete [] buf_zip;
    return ret;
}


/**
 * @brief write block to disk
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int writeblock(s_job_t * jo, const u_char * buf, s_block_t * blk)
{
    GKO_INT64 f = blk->start_f;
    off_t offset = blk->start_off;
    GKO_INT64 total = blk->size;
    GKO_INT64 counter = 0;
    GKO_INT64 file_size;

    while (total > 0)
    {
        GKO_INT64 wrote;
        s_file_t * file = jo->files + f;
        file_size = file->size;
        int fd = open(file->name, WRITE_OPEN_FLAG);
        if (fd < 0)
        {
            gko_log(WARNING, "open file '%s' for write error", file->name);
            return -2;
        }
        wrote = pwrite(fd, buf + counter, MIN(file_size - offset, total),
                offset);
        if (wrote < 0)
        {
            gko_log(WARNING, "write file error");
            return -1;
        }
        disk_w_limit(wrote, gko.opt.limit_disk_w_rate);
        total -= wrote;
        counter += wrote;
        fsync(fd);
        close(fd);
        f = next_f(jo, f);
        offset = 0;
    }
    return counter;
}

/**
 * @brief send cmd msg to host, not read response, on succ return 0
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int sendcmd2host(const s_host_t *h, const char * cmd, const int recv_sec, const int send_sec)
{
    int sock;
    int result;
    int msg_len;
    char new_cmd[MSG_LEN] = {'\0'};

    sock = connect_host(h, recv_sec, send_sec);
    if (sock < 0)
    {
        gko_log(DEBUG, "sendcmd2host() connect_host error");
        return -1;
    }
    msg_len = snprintf(new_cmd, sizeof(new_cmd), "%s%s", PREFIX_CMD, cmd);
    if (msg_len <= CMD_PREFIX_BYTE)
    {
        gko_log(WARNING, FLF("snprintf error"));
        close_socket(sock);
        return -1;
    }
    else
    {
        fill_cmd_head(new_cmd, msg_len);
    }
    result = sendall(sock, new_cmd, msg_len, send_sec);

    close_socket(sock);
    return result;
}

/**
 * quit func
 */
void gko_quit(int ret)
{
    lock_log();
    //gko_pool::getInstance()->gko_loopexit(0);
    usleep(100000);
    _exit(ret);
}



