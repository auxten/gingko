/*
 * clnt_unittest.cpp
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */

#define UNITTEST
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


/************** PTHREAD STUFF **************/
///default pthread_attr_t
pthread_attr_t g_attr;
///client wide lock
pthread_mutex_t g_clnt_lock;
///block host set lock
pthread_mutex_t g_blk_hostset_lock;
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

#include "gtest/gtest.h"
#include "unittest.h"

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT


int clnt_unittest_init()
{
    memset(&g_job, 0, sizeof(g_job));
    memset(&gko, 0, sizeof(gko));
    memset(&gko.the_serv, 0, sizeof(gko.the_serv));

    gko.opt.limit_up_rate = CLNT_LIMIT_UP_RATE;
    gko.opt.limit_down_rate = CLNT_LIMIT_DOWN_RATE;
    gko.opt.worker_thread = CLNT_ASYNC_THREAD_NUM;
    gko.opt.connlimit = CLNT_POOL_SIZE;
    gko.opt.bind_ip = htons(INADDR_ANY);
    gko.the_serv.port = SERV_PORT;
    gko.ready_to_serv = 0;
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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(inplace_strip_tailing_slash, with1slash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa/", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

TEST(inplace_strip_tailing_slash, withmoreslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa////", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

TEST(inplace_strip_tailing_slash, withnoslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

/**
 * @brief ../path///    TO  ../path/
 * @brief ../path/     TO  ../path/
 * @brief ../path      TO  ../path/
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(inplace_add_tailing_slash, with1slash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa/", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

TEST(inplace_add_tailing_slash, withmoreslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa////", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

TEST(inplace_add_tailing_slash, withnoslash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa", MAX_PATH_LEN);
    char * test_rslt = "aaa/";
    inplace_add_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(get_base_name_index, 1)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "/home/work/opdir"), 11);
    EXPECT_STREQ("opdir", test_path1);
}

TEST(get_base_name_index, 2)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "./dir"), 2);
    EXPECT_STREQ("dir", test_path1);
}

TEST(get_base_name_index, 3)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "dir"), 0);
    EXPECT_STREQ("dir", test_path1);
}

TEST(get_base_name_index, 4)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "../file"), 3);
    EXPECT_STREQ("file", test_path1);
}

TEST(get_base_name_index, 5)
{
    char test_path1[MAX_PATH_LEN];

    EXPECT_EQ(get_base_name_index(test_path1, "../file/"), 8);
    EXPECT_STREQ("", test_path1);
}

/**
 * @brief in : const char * dir_name, const char * base_name
 * @brief out : dir_name/base_name
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(merge_path, normal)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(0, merge_path(result, "dirname", "basename"));
    EXPECT_STREQ("dirname/basename", result);
}

TEST(merge_path, dirwithslash)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(0, merge_path(result, "dirname/", "basename"));
    EXPECT_STREQ("dirname/basename", result);
}

TEST(merge_path, dirwith2slash)
{
    char result[MAX_PATH_LEN];
    EXPECT_EQ(0, merge_path(result, "dirname//", "basename"));
    EXPECT_STREQ("dirname/basename", result);
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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(change_to_local_path, dst_path_exist)
{
    char path[MAX_PATH_LEN];
    strncpy(path, "../test/.DS_Store", MAX_PATH_LEN);
    EXPECT_EQ(0, change_to_local_path(path, "../test", "../output2/", 1));
    EXPECT_STREQ("../output2/test/.DS_Store", path);
}

TEST(change_to_local_path, dst_path_nonexist)
{
    char path[MAX_PATH_LEN];
    strncpy(path, "../test/.DS_Store", MAX_PATH_LEN);
    EXPECT_EQ(0, change_to_local_path(path, "../test", "../output2/", 0));
    EXPECT_STREQ("../output2/.DS_Store", path);
}

/**
 * @brief change current working dir path to absolute path
 *
 * @see
 * @note
 *     result is stored in abs_path
 *     return abs_path on succeed else NULL
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(cwd_path_to_abs_path, relative_path)
{
    char path[MAX_PATH_LEN];
    char orig_path[MAX_PATH_LEN];
    EXPECT_NE((char *)NULL, getcwd(orig_path, MAX_PATH_LEN));
    EXPECT_EQ(0, chdir("/home"));
    EXPECT_NE((char *)NULL, cwd_path_to_abs_path(path, "work"));
    EXPECT_STREQ("/home/work", path);
    EXPECT_EQ(0, chdir(orig_path));
}

TEST(cwd_path_to_abs_path, abs_path)
{
    char path[MAX_PATH_LEN];
    EXPECT_NE((char *)NULL, cwd_path_to_abs_path(path, "/work"));
    EXPECT_STREQ("/work", path);
}

/**
 * @brief get the symlink dest's absolute path, store it in abs_path
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(symlink_dest_to_abs_path, symlink_to_relative_path)
{
    char path[MAX_PATH_LEN];
    char path2[MAX_PATH_LEN];
    EXPECT_NE((char *)NULL, getcwd(path2, MAX_PATH_LEN));
    strncat(path2, "/../testcase/test.sh", MAX_PATH_LEN);
    EXPECT_NE((char *)NULL, symlink_dest_to_abs_path(path, "../testcase/test.sh.ln"));
    EXPECT_STREQ(path2, path);
}

TEST(symlink_dest_to_abs_path, symlink_to_abs_path)
{
    char path[MAX_PATH_LEN];
    EXPECT_NE((char *)NULL, symlink_dest_to_abs_path(path, "../testcase/test.sh.absln"));
    EXPECT_STREQ("/home/auxten", path);
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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
TEST(socket, Connect_host_Setnonblock_Setblock_Close_socket)
{
    int fd, flags;
    s_host_t h = {"localhost", 22};

    ///establish socket
    fd = connect_host(&h, 5, 5);
    EXPECT_LE(0, fd);

    ///setnonblock
    EXPECT_EQ(0, setnonblock(fd));
    flags = fcntl(fd, F_GETFL);
    EXPECT_LE(0, flags);
    EXPECT_NE(0, flags & O_NONBLOCK);

    ///try setnonblock again
    EXPECT_EQ(0, setnonblock(fd));
    flags = fcntl(fd, F_GETFL);
    EXPECT_LE(0, flags);
    EXPECT_NE(0, flags & O_NONBLOCK);

    ///setblock
    EXPECT_EQ(0, setblock(fd));
    flags = fcntl(fd, F_GETFL);
    EXPECT_LE(0, flags);
    EXPECT_EQ(0, flags & O_NONBLOCK);

    ///try setblock again
    EXPECT_EQ(0, setblock(fd));
    flags = fcntl(fd, F_GETFL);
    EXPECT_LE(0, flags);
    EXPECT_EQ(0, flags & O_NONBLOCK);

    ///close socket
    EXPECT_EQ(0, close_socket(fd));
}


///options.cpp
/**
 * @brief process args for client
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
//int clnt_parse_opt(int argc, char *argv[], s_job_t * jo)
TEST(clnt_parse_opt, clnt_parse_opt)
{
    const int max_argc = 30;
    char argv_test_buf[max_argc][MAX_PATH_LEN] =
    {
        "gkocp",
        "-l",
        "./clnt.log",
        "127.0.0.1:./src",
        "./dest",
        "-o", "-c", "-u", "17", "-d", "18", "-r", "19", "-w", "20",
        "-t", "21", "-n", "22", "-s", "23", "-b", "127.0.0.1", "-p", "2121", "--debug"
    };
    char * argv_test[max_argc];
    for (int i = 0; i < max_argc; i++)
    {
        argv_test[i] = (char *)argv_test_buf[i];
    }
    int argc_test = 26;
    EXPECT_EQ(0, clnt_parse_opt(argc_test, argv_test, &g_job));
    //EXPECT_EQ(htonl(INADDR_LOOPBACK), gko.opt.bind_ip);
    EXPECT_EQ(22, gko.opt.connlimit);
    EXPECT_EQ(17*1024*1024, gko.opt.limit_up_rate);
    EXPECT_EQ(18*1024*1024, gko.opt.limit_down_rate);
    EXPECT_EQ(19*1024*1024, gko.opt.limit_disk_r_rate);
    EXPECT_EQ(20*1024*1024, gko.opt.limit_disk_w_rate);
    EXPECT_EQ(0, gko.opt.need_help);
    EXPECT_EQ(1, gko.opt.need_progress);
    EXPECT_EQ(2121, gko.the_serv.port);
    EXPECT_EQ(23, gko.opt.seed_time);
    EXPECT_EQ(1, gko.opt.to_continue);
    EXPECT_EQ(1, gko.opt.to_debug);
    EXPECT_EQ(21, gko.opt.worker_thread);
    EXPECT_STREQ("./clnt.log", gko.opt.logpath);

}

TEST(clnt_show_version, clnt_show_version)
{
    EXPECT_NO_FATAL_FAILURE(clnt_show_version());
}

TEST(clnt_show_help, clnt_show_help)
{
    EXPECT_NO_FATAL_FAILURE(clnt_show_help());
}

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
