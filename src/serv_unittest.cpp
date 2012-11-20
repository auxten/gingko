/*
 * serv_unittest.cpp
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */

#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /** GINGKO_SERV **/

#include "gingko.h"
#include "async_pool.h"
#include "hash/xor_hash.h"
#include "path.h"
#include "log.h"
#include "seed.h"
#include "socket.h"
#include "option.h"
#include "job_state.h"
#include "gingko_serv.h"

#define UNITTEST
/************** PTHREAD STUFF **************/
///server wide lock
pthread_mutex_t g_grand_lock;
///job specific lock
s_lock_t g_job_lock[MAX_JOBS];
pthread_key_t g_dir_key;
/************** PTHREAD STUFF **************/
/// jobs map
std::map<std::string, s_job_t *> g_m_jobs;

/// gingko global stuff
s_gingko_global_t gko;

#ifdef UNITTEST

#include "gtest/gtest.h"
#include "unittest.h"

/************** FUNC DICT **************/
#include "gingko_common.h"
/************** FUNC DICT **************/

GINGKO_OVERLOAD_S_HOST_LT


TEST(inplace_strip_tailing_slash, with1slash)
{
    char test_path1[MAX_PATH_LEN];
    strncpy(test_path1, "aaa/", MAX_PATH_LEN);
    char * test_rslt = "aaa";
    inplace_strip_tailing_slash(test_path1);
    EXPECT_STREQ(test_rslt, test_path1);
}

/**
 * @brief process args for server
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
//int serv_parse_opt(int argc, char *argv[])
TEST(serv_parse_opt, serv_parse_opt)
{
    const int max_argc = 30;
    char argv_test_buf[max_argc][MAX_PATH_LEN] =
    {
        "gkod",
        "-l", "./serv.log",
        "-u", "17", "-r", "19",
        "-t", "21", "-n", "22", "-b", "127.0.0.1",
        "-p", "2121", "--debug"
    };
    char * argv_test[max_argc];
    for (int i = 0; i < max_argc; i++)
    {
        argv_test[i] = (char *)argv_test_buf[i];
    }
    int argc_test = 16;
    EXPECT_EQ(0, serv_parse_opt(argc_test, argv_test));
    //EXPECT_EQ(htonl(INADDR_LOOPBACK), gko.opt.bind_ip);
    EXPECT_EQ(22, gko.opt.connlimit);
    EXPECT_EQ(17*1024*1024, gko.opt.limit_up_rate);
    EXPECT_EQ(19*1024*1024, gko.opt.limit_disk_r_rate);
    EXPECT_EQ(0, gko.opt.need_help);
    EXPECT_EQ(0, gko.opt.daemon_mode);
    EXPECT_EQ(2121, gko.opt.port);
    EXPECT_EQ(1, gko.opt.to_debug);
    EXPECT_EQ(21, gko.opt.worker_thread);
    EXPECT_STREQ("./serv.log", gko.opt.logpath);

}

TEST(serv_show_version, serv_show_version)
{
    EXPECT_NO_FATAL_FAILURE(serv_show_version());
}

TEST(serv_show_help, serv_show_help)
{
    EXPECT_NO_FATAL_FAILURE(serv_show_help());
}


/// test thread pool
TEST(async_threads, gko_pool)
{
    gko.ready_to_serv = 1;
    gko.sig_flag = 0;

    gko_log(DEBUG, "Debug mode start, i will print tons of log :p!");

//    if (pthread_init() != 0)
//    {
//        gko_log(FATAL, FLF("pthread_init error"));
//        fprintf(stderr, "Server error, quited\n");
//        gko_quit(1);
//    }

    gko_pool * gingko = gko_pool::getInstance();
    EXPECT_NE((gko_pool*)NULL, gingko);
    EXPECT_NO_FATAL_FAILURE(gingko->setPort(2120));
    EXPECT_NO_FATAL_FAILURE(gingko->setOption(&gko.opt));
    EXPECT_NO_FATAL_FAILURE(
            gingko->setFuncTable(g_cmd_list, g_func_list_s, CMD_COUNT));

    EXPECT_NO_FATAL_FAILURE(gingko->gko_loopexit(2));
    EXPECT_NO_FATAL_FAILURE(gingko->gko_run());
}

/**
 * @brief server unittest main
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-15
 **/
int main(int argc, char** argv)
{
    strncpy(gko.opt.logpath, "/dev/stdout", MAX_PATH_LEN);
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
#else
int main()
{

}
#endif /* UNITTEST */
