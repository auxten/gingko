/*
 * clnt_unittest.cpp
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */


#ifndef GINGKO_CLNT
#define GINGKO_CLNT
#endif /** GINGKO_CLNT **/

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
#include <getopt.h>
#include <inttypes.h>
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/sendfile.h>
#endif /** __APPLE__ **/

#include "gingko.h"
#include "async_pool.h"
#include "hash/xor_hash.h"
#include "path.h"
#include "route.h"
#include "log.h"
#include "snap.h"
#include "option.h"
#include "socket.h"
#include "limit.h"
#include "job_state.h"
#include "gingko_clnt.h"

#include "gtest/gtest.h"
#include "unittest.h"


/************** PTHREAD STUFF **************/
///default pthread_attr_t
pthread_attr_t g_attr;
///client wide lock
pthread_rwlock_t g_clnt_lock;
///block host set lock
pthread_rwlock_t g_blk_hostset_lock;
///mutex for gko.hosts_new_noready
pthread_mutex_t g_hosts_new_noready_mutex;
///mutex for gko.hosts_del_noready
pthread_mutex_t g_hosts_del_noready_mutex;
/************** PTHREAD STUFF **************/

/// the g_job assoiate with the client
s_job_t g_job;

/// gingko global stuff
s_gingko_global_t gko;

#ifdef UNITTEST

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT
GINGKO_OVERLOAD_S_HOST_EQ

using namespace std;

int clnt_unittest_init()
{
    memset(&g_job, 0, sizeof(g_job));
    memset(&gko, 0, sizeof(gko));
    memset(&gko.the_clnt, 0, sizeof(gko.the_clnt));
    memset(&gko.the_serv, 0, sizeof(gko.the_serv));

    gko.opt.limit_up_rate = CLNT_LIMIT_UP_RATE;
    gko.opt.limit_down_rate = CLNT_LIMIT_DOWN_RATE;
    gko.opt.worker_thread = CLNT_ASYNC_THREAD_NUM;
    gko.opt.connlimit = CLNT_POOL_SIZE;
    gko.opt.seed_time = SEED_TIME;
    gko.opt.bind_ip = htons(INADDR_ANY);
    gko.the_serv.port = SERV_PORT;
    gko.ready_to_serv = 0;
    gko.cmd_list_p = g_cmd_list;
    gko.func_list_p = g_func_list_s;
    gko.snap_fd = -2;
    strncpy(gko.opt.logpath, CLIENT_LOG, sizeof(gko.opt.logpath));

    umask(0);

    return 0;
}


/// path.cpp
/**
 * @brief  ../path////   TO  ../path
 * @brief  ../path/     TO  ../path
 * @brief  ../path      TO  ../path
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(inplace_strip_tailing_slash, with1slash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa/", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

TEST(inplace_strip_tailing_slash, withmoreslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa////", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

TEST(inplace_strip_tailing_slash, withnoslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

/**
 * @brief ../path///    TO  ../path/
 * @brief ../path/     TO  ../path/
 * @brief ../path      TO  ../path/
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(inplace_add_tailing_slash, with1slash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa/", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

TEST(inplace_add_tailing_slash, withmoreslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa////", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

TEST(inplace_add_tailing_slash, withnoslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_path1, test_rslt);
}

/**
 * @brief  get the base name of a string,
 * @brief  if out not NULL, cp the base path to out
 * @brief  return the base path len
 *
 * @see
 * @note
 * for example:
 *     /home/work/opdir ->  11
 *                ^
 *     ./dir            ->  2
 *       ^
 *     dir              ->  0
 *     ^
 *     ../file          ->  3
 *        ^
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(get_base_name_index, 1)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "/home/work/opdir"), 11);
    EXPECT_STREQ(test_path1, "opdir");
}

TEST(get_base_name_index, 2)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "./dir"), 2);
    EXPECT_STREQ(test_path1, "dir");
}

TEST(get_base_name_index, 3)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "dir"), 0);
    EXPECT_STREQ(test_path1, "dir");
}

TEST(get_base_name_index, 4)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "../file"), 3);
    EXPECT_STREQ(test_path1, "file");
}

TEST(get_base_name_index, 5)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "../file/"), 8);
    EXPECT_STREQ(test_path1, "");
}

/**
 * @brief in : const char * dir_name, const char * base_name
 * @brief out : dir_name/base_name
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(merge_path, normal)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(merge_path(result, "dirname", "basename"), 0);
    EXPECT_STREQ(result, "dirname/basename");
}

TEST(merge_path, dirwithslash)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(merge_path(result, "dirname/", "basename"), 0);
    EXPECT_STREQ(result, "dirname/basename");
}

TEST(merge_path, dirwith2slash)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(merge_path(result, "dirname//", "basename"), 0);
    EXPECT_STREQ(result, "dirname/basename");
}

/**
 * @brief  change remote path to local path, store it in path
 *
 * @see
 * @note
 *     path: ../test/.DS_Store
 *     req_path:    ../test
 *     local_path: ../output2/
 *     if (dst_path_exist)
 *          path output:  ../output2/test/.DS_Store
 *     else
 *          path output:  ../output2/.DS_Store
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(change_to_local_path, dst_path_exist)
{
    char path[MAX_PATH_LEN];
    strncpy(path, "../test/.DS_Store", MAX_PATH_LEN);
    EXPECT_EQ(change_to_local_path(path, "../test", "../output2/", 1), 0);
    EXPECT_STREQ(path, "../output2/test/.DS_Store");
}

TEST(change_to_local_path, dst_path_nonexist)
{
    char path[MAX_PATH_LEN];
    strncpy(path, "../test/.DS_Store", MAX_PATH_LEN);
    EXPECT_EQ(change_to_local_path(path, "../test", "../output2/", 0), 0);
    EXPECT_STREQ(path, "../output2/.DS_Store");
}

/**
 * @brief change current working dir path to absolute path
 *
 * @see
 * @note
 *     result is stored in abs_path
 *     return abs_path on succeed else NULL
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(cwd_path_to_abs_path, relative_path)
{
    char path[MAX_PATH_LEN];
    char orig_path[MAX_PATH_LEN];
    EXPECT_NE(getcwd(orig_path, MAX_PATH_LEN), (char *)NULL);
    EXPECT_EQ(chdir("/home"), 0);
    EXPECT_NE(cwd_path_to_abs_path(path, "work"), (char *)NULL);
    EXPECT_STREQ(path, "/home/work");
    EXPECT_EQ(chdir(orig_path), 0);
}

TEST(cwd_path_to_abs_path, abs_path)
{
    char path[MAX_PATH_LEN];
    EXPECT_NE(cwd_path_to_abs_path(path, "/work"), (char *)NULL);
    EXPECT_STREQ(path, "/work");
}

/**
 * @brief get the symlink dest's absolute path, store it in abs_path
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(symlink_dest_to_abs_path, symlink_to_relative_path)
{
    char path[MAX_PATH_LEN];
    char path2[MAX_PATH_LEN];
    EXPECT_NE(getcwd(path2, MAX_PATH_LEN), (char *)NULL);
    strncat(path2, "/../testcase/test.sh", MAX_PATH_LEN);
    EXPECT_NE(symlink_dest_to_abs_path(path, "../testcase/test.sh.ln"), (char *)NULL);
    EXPECT_STREQ(path, path2);
}

TEST(symlink_dest_to_abs_path, symlink_to_abs_path)
{
    char path[MAX_PATH_LEN];
    EXPECT_NE(symlink_dest_to_abs_path(path, "../testcase/test.sh.absln"), (char *)NULL);
    EXPECT_STREQ(path, "/home/auxten");
}

///socket.cpp
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
TEST(socket, Connect_host_Setnonblock_Setblock_Close_socket)
{
    int fd, flags;
    s_host_t h = {"localhost", 22};

    ///establish socket
    fd = connect_host(&h, 5, 5);
    EXPECT_GE(fd, 0);

    ///setnonblock
    EXPECT_EQ(setnonblock(fd), 0);
    flags = fcntl(fd, F_GETFL);
    EXPECT_GE(flags, 0);
    EXPECT_NE(flags & O_NONBLOCK, 0);

    ///try setnonblock again
    EXPECT_EQ(setnonblock(fd), 0);
    flags = fcntl(fd, F_GETFL);
    EXPECT_GE(flags, 0);
    EXPECT_NE(flags & O_NONBLOCK, 0);

    ///setblock
    EXPECT_EQ(setblock(fd), 0);
    flags = fcntl(fd, F_GETFL);
    EXPECT_GE(flags, 0);
    EXPECT_EQ(flags & O_NONBLOCK, 0);

    ///try setblock again
    EXPECT_EQ(setblock(fd), 0);
    flags = fcntl(fd, F_GETFL);
    EXPECT_GE(flags, 0);
    EXPECT_EQ(flags & O_NONBLOCK, 0);

    ///close socket
    EXPECT_EQ(close_socket(fd), 0);
}


///options.cpp
/**
 * @brief process args for client
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
//int clnt_parse_opt(int argc, char *argv[], s_job_t * jo)


int main(int argc, char *argv[])
{
    if(clnt_unittest_init() != 0)
    {
        perror("clnt_unittest_init failed");
        exit(1);
    }
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
#else
int main()
{

}
#endif /* UNITTEST */
