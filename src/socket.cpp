/**
 * socket.cpp
 *
 *  Created on: 2011-7-17
 *      Author: auxten
 **/

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>

#include "config.h"
#include "gingko.h"
#include "log.h"
#include "socket.h"
#ifdef __APPLE__
#include <poll.h>
#else
#include <sys/poll.h>
#endif /** __APPLE__ **/


/**
 * @brief Set non-blocking
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int setnonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        return flags;
    }

    if (!(flags & O_NONBLOCK))
    {
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0)
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Set blocking
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int setblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        return flags;
    }

    if (flags & O_NONBLOCK)
    {
        flags &= ~O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) < 0)
        {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief connect to a host
 *
 * @see
 * @note
 *     h: pointer to s_host_t
 *     recv_sec: receive timeout seconds, 0 for never timeout
 *     return the socket when succ
 *     return < 0 when error, specially HOST_DOWN_FAIL indicate host dead
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int connect_host(s_host_t * h, int recv_sec, int send_sec)
{
    int sock = -1;
    int ret;
    int select_ret;
    int res;
    socklen_t res_size = sizeof res;
    struct sockaddr_in channel;
    in_addr_t host;
    int addr_len;
    struct timeval recv_timeout;
    struct timeval send_timeout;
    fd_set wset;

    addr_len = getaddr_my(h->addr, &host);
    if (FAIL_CHECK(!addr_len))
    {
        gko_log(WARNING, "gethostbyname %s error", h->addr);
        ret = -1;
        goto CONNECT_END;
    }
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (FAIL_CHECK(sock < 0))
    {
        gko_log(WARNING, "get socket error");
        ret = -1;
        goto CONNECT_END;
    }

    recv_timeout.tv_usec = 0;
    recv_timeout.tv_sec = recv_sec ? recv_sec : RCV_TIMEOUT;
    send_timeout.tv_usec = 0;
    send_timeout.tv_sec = send_sec ? send_sec : SND_TIMEOUT;

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    memcpy(&channel.sin_addr.s_addr, &host, addr_len);
    channel.sin_port = htons(h->port);

    /** set the connect non-blocking then blocking for add timeout on connect **/
    if (FAIL_CHECK(setnonblock(sock) < 0))
    {
        gko_log(WARNING, "set socket non-blocking error");
        ret = -1;
        goto CONNECT_END;
    }

    /** connect and send the msg **/
    if (FAIL_CHECK(connect(sock, (struct sockaddr *) &channel, sizeof(channel)) &&
            errno != EINPROGRESS))
    {
        gko_log(WARNING, "connect error");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    /** Wait for write bit to be set **/
#if HAVE_POLL
    {
        struct pollfd pollfd;

        pollfd.fd = sock;
        pollfd.events = POLLOUT;

        /* send_sec is in seconds, timeout in ms */
        select_ret = poll(&pollfd, 1, (int)(send_sec * 1000 + 1));
    }
#else
    {
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        select_ret = select(sock + 1, 0, &wset, 0, &send_timeout);
    }
#endif /* HAVE_POLL */
    if (select_ret < 0)
    {
        gko_log(FATAL, "select/poll error on connect");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }
    if (!select_ret)
    {
        gko_log(FATAL, "connect timeout on connect");
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    /**
     * check if connection is RESETed, maybe this is the
     * best way to do that
     * SEE: http://cr.yp.to/docs/connect.html
     **/
    (void) getsockopt(sock, SOL_SOCKET, SO_ERROR, &res, &res_size);
    if (CONNECT_DEST_DOWN(res))
    {
        gko_log(NOTICE, "connect dest is down errno: %d", res);
        ret = HOST_DOWN_FAIL;
        goto CONNECT_END;
    }

    ///gko_log(WARNING, "selected %d ret %d, time %d", sock, select_ret, send_timeout.tv_sec);
    /** set back blocking **/
    if (FAIL_CHECK(setblock(sock) < 0))
    {
        gko_log(WARNING, "set socket non-blocking error");
        ret = -1;
        goto CONNECT_END;
    }

    /** set recv & send timeout **/
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &recv_timeout,
                    sizeof(struct timeval))))
    {
        gko_log(WARNING, "setsockopt SO_RCVTIMEO error");
        ret = -1;
        goto CONNECT_END;
    }
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &send_timeout,
                    sizeof(struct timeval))))
    {
        gko_log(WARNING, "setsockopt SO_SNDTIMEO error");
        ret = -1;
        goto CONNECT_END;
    }

    ret = sock;

    CONNECT_END:
    ///
    if (ret < 0 && sock >= 0)
    {
        close_socket(sock);
    }
    return ret;
}

/**
 * @brief gracefully close a socket, for client side
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int close_socket(int sock)
{
    ///  if (shutdown(sock, 2)) {
    ///      gko_log(WARNING, "shutdown sock error");
    ///      return -1;
    ///  }
    struct linger so_linger;
    so_linger.l_onoff = 1; /// close right now, no time_wait at serv
    so_linger.l_linger = 0; /// at most wait for 1s
    if (FAIL_CHECK(setsockopt(sock, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger))))
    {
        gko_log(WARNING, "set so_linger failed");
    }
    if (FAIL_CHECK(close(sock)))
    {
        gko_log(WARNING, "close sock error");
        return -1;
    }
    return 0;
}

