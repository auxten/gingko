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
    EXPECT_STREQ(test_path1, test_rslt);
}

/**
 * @brief server unittest main
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
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
