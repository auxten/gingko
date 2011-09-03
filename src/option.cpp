/**
 * options.cpp
 *
 *  Created on: 2011-7-14
 *      Author: auxten
 **/
#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /** GINGKO_SERV **/
#ifndef GINGKO_CLNT
#define GINGKO_CLNT
#endif /** GINGKO_CLNT **/

#include <getopt.h>
#include <inttypes.h>
#include <netdb.h>

#include "gingko.h"
#include "option.h"
#include "path.h"
#include "log.h"

extern s_gingko_global_t gko;

/**
 * @brief print version info
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void clnt_show_version()
{
    printf("gingko_clnt %s\n", GKO_VERSION);
    return;
}

/**
 * @brief print version info
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void serv_show_version()
{
    printf("gingko_serv %s\n", GKO_VERSION);
    return;
}

/**
 * @brief print help info for client
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void clnt_show_help()
{
    printf(
            "\n\
NAME\n\
     gingko_clnt -- p2p copy (remote dir copy program with p2p transfer support)\n\
\n\
SYNOPSIS\n\
     gingko_clnt [options] host1:dir1 dir2\n\
\n\
DESCRIPTION\n\
     gingko_clnt copies dir1 from gingko_serv to local dir2. It uses p2p for data transfer.\n\
\n\
     I tried my best to make it compatible with scp in usage, gingko_clnt will preserve \n\
     the 'rwx' attributes. The recursive mode is default on.\n\
\n\
     BUT NOTICE that:\n\
       the last level of source path should not be soft link, ie: /path1/path2/path3\n\
     path3 should not be soft link.\n\
       we do not follow any soft link in src in dir\n\
\n\
     The options are as follows: (LONG OPTIONS is recommonded)\n\
\n\
     -h\n\
     --help\n\
         Show this help message.\n\
\n\
     -c\n\
     --continue\n\
         Continue getting a partially-downloaded job. default is not enabled\n\
\n\
     -u upload_limit_num\n\
     --uplimit=upload_limit_num\n\
         Limit the upload bandwidth to limit_num MB/s. default is %d MB/s\n\
\n\
     -d download_limit_num\n\
     --downlimit=download_limit_num\n\
         Limit the download bandwith to limit_num MB/s. default is %d MB/s\n\
\n\
     -t worker_thread_num\n\
     --workerthread=worker_thread_num\n\
         Worker threads num. default is %d threads\n\
\n\
     -n connection_limit_num\n\
     --connlimit=connection_limit_num\n\
         Upload connection num. default is %d connections\n\
\n\
     -s seed_time\n\
     --seedtime=seed_time_num\n\
         After downloading continue seed time. default is %d seconds\n\
\n\
     -b hostname\n\
     --bind=hostname\n\
         Bind IP, default is 0.0.0.0.\n\
\n\
     -p portnum\n\
     --port=portnum\n\
         Port to connect the server. default is %d \n\
\n\
     -l logpath\n\
     --log=logpath\n\
         Path for log file. default is %s\n\
\n\
     -v\n\
     --version\n\
         Show version message\n\
\n\
EXAMPLES\n\
     The following is how to copy the /path/to/data_src_dir from yf-cm-gingko00.yf01 to localhost\n\
     /path/to/data_dest_dir, the upload bandwidth and download bandwidth is limited to 10MB/s and\n\
     the continue transfer is also enabled.\n\
\n\
     gingko_clnt -u 10 -d 10 -c yf-cm-gingko00.yf01:/path/to/data_src_dir /path/to/data_dest_dir\n\
\n\
AUTHORS\n\
     Wang Pengcheng <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>\n\
",
            CLNT_LIMIT_UP_RATE / 1024 / 1024,
            CLNT_LIMIT_DOWN_RATE / 1024 / 1024,
            CLNT_ASYNC_THREAD_NUM,
            CLNT_POOL_SIZE,
            SEED_TIME,
            SERV_PORT,
            CLIENT_LOG);
}

/**
 * @brief print server help info
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void serv_show_help()
{
    printf(
            "\n\
NAME\n\
     gingko_serv -- p2p copy server side (remote dir copy program with p2p transfer support)\n\
\n\
SYNOPSIS\n\
     gingko_serv [options]\n\
\n\
DESCRIPTION\n\
     gingko_serv is the server side of gingko. It uses p2p for data transfer.\n\
\n\
     The options are as follows: (LONG OPTIONS is recommonded)\n\
\n\
     -h\n\
     --help\n\
         Show this help message.\n\
\n\
     -u upload_limit_num\n\
     --uplimit=upload_limit_num\n\
         Limit the upload bandwidth to limit_num MB/s. default is %d MB/s\n\
\n\
     -t worker_thread_num\n\
     --connlimit=worker_thread_num\n\
         Worker threads num. default is %d threads\n\
\n\
     -n connection_limit_num\n\
     --connlimit=connection_limit_num\n\
         Upload connection num. default is %d connections\n\
\n\
     -b hostname\n\
     --bind=hostname\n\
         Bind IP, default is 0.0.0.0.\n\
\n\
     -p portnum\n\
     --port=portnum\n\
         Port to listen. default is %d\n\
\n\
     -l logpath\n\
     --log=logpath\n\
         Path for log file. default is %s\n\
\n\
     DEVELOPING: -e seed_speed_limit_num\n\
     --seedspeed=seed_speed_limit_num\n\
         Server make seed speed in MB/s.\n\
\n\
     -v\n\
     --version\n\
         Show version message\n\
\n\
\n\
AUTHORS\n\
     Wang Pengcheng <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>\n\
",
            SERV_LIMIT_UP_RATE / 1024 / 1024,
            SERV_ASYNC_THREAD_NUM,
            SERV_POOL_SIZE,
            SERV_PORT,
            SERVER_LOG);
}

/**
 * @brief process args for client
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int clnt_parse_opt(int argc, char *argv[], s_job_t * jo)
{

    struct option long_options[] =
        {
            { "continue", no_argument, 0, 'c' },
            { "help", no_argument, 0, 'h' },
            { "uplimit", required_argument, 0, 'u' },
            { "downlimit", required_argument, 0, 'd' },
            { "workerthread", required_argument, 0, 't' },
            { "connlimit", required_argument, 0, 'n' },
            { "seedtime", required_argument, 0, 's' },
            { "bind", required_argument, 0, 'b' },
            { "port", required_argument, 0, 'p' },
            { "log", required_argument, 0, 'l' },
            { "version", no_argument, 0, 'v' },
            { 0, 0, 0, 0 } };

    /**
     * set default up down rate limit
     **/
    gko.opt.limit_up_rate = CLNT_LIMIT_UP_RATE;
    gko.opt.limit_down_rate = CLNT_LIMIT_DOWN_RATE;
    gko.opt.worker_thread = CLNT_ASYNC_THREAD_NUM;
    gko.opt.connlimit = CLNT_POOL_SIZE;
    gko.opt.seed_time = SEED_TIME;
    gko.opt.bind_ip = htons(INADDR_ANY);
    gko.the_serv.port = SERV_PORT;
    strncpy(gko.opt.logpath, CLIENT_LOG, sizeof(gko.opt.logpath));

    /** process args **/
    while (1)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "chu:d:t:n:s:b:p:l:v", long_options,
                &option_index);
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 0: /** if 3rd field of struct option is non zero **/
                fprintf(stderr, "option %s with arg %s\n",
                        long_options[option_index].name, optarg ? optarg : " ");
                break;

            case 'h':
                gko.opt.need_help = 1;
                clnt_show_help();
                exit(0);
                break;

            case 'c':
                gko.opt.to_continue = 1;
                break;

            case 'u':
                gko.opt.limit_up_rate = strtoimax(optarg, NULL, 10) * 1024 * 1024;
                if (gko.opt.limit_up_rate <= 0 || gko.opt.limit_up_rate > 1000 * 1024
                        * 1024)
                {
                    fprintf(stderr, "Invalid uplimit num: %d\n",
                            gko.opt.limit_up_rate);
                    exit(1);
                }
                fprintf(stderr, "gko.opt.limit_up_rate: %d\n", gko.opt.limit_up_rate);
                break;

            case 'd':
                gko.opt.limit_down_rate = strtoimax(optarg, NULL, 10) * 1024 * 1024;
                if (gko.opt.limit_down_rate <= 0 || gko.opt.limit_down_rate > 1000
                        * 1024 * 1024)
                {
                    fprintf(stderr, "Invalid downlimit num: %d\n",
                            gko.opt.limit_down_rate);
                    exit(1);
                }
                fprintf(stderr, "gko.opt.limit_down_rate: %d\n", gko.opt.limit_down_rate);
                break;

            case 't':/** worker_thread **/
                gko.opt.worker_thread = strtoimax(optarg, NULL, 10);
                if (gko.opt.worker_thread < 1 || gko.opt.worker_thread > 2047
                        * 1024 * 1024)
                {
                    fprintf(stderr, "Invalid worker_thread num: %d\n",
                            gko.opt.worker_thread);
                    exit(1);
                }
                break;

            case 'n':/** connlimit **/
                gko.opt.connlimit = strtoimax(optarg, NULL, 10);
                if (gko.opt.connlimit < 1 || gko.opt.connlimit > 2047 * 1024
                        * 1024)
                {
                    fprintf(stderr, "Invalid connlimit num: %d\n",
                            gko.opt.connlimit);
                    exit(1);
                }
                break;

            case 's':/** seedtime **/
                gko.opt.seed_time = strtoimax(optarg, NULL, 10);
                if (gko.opt.seed_time < 1 || gko.opt.seed_time > 2047
                        * 1024 * 1024)
                {
                    fprintf(stderr, "Invalid seed_time num: %d\n",
                            gko.opt.seed_time);
                    exit(1);
                }
                break;

            case 'b':/** bind ip **/
                in_addr_t serv;
                int addr_len;
                addr_len = getaddr_my(optarg, &serv);
                if (!addr_len)
                {
                    fprintf(stderr, "Can't resolve src server\n");
                    exit(1);
                }
                memcpy(&gko.opt.bind_ip, &serv, addr_len);
                break;

            case 'p':/** port **/
                gko.the_serv.port = strtoimax(optarg, NULL, 10);
                if (gko.the_serv.port < MIN_PORT || gko.the_serv.port
                        > MAX_PORT)
                {
                    fprintf(stderr, "Invalid port num: %d\n", gko.the_serv.port);
                    exit(1);
                }
                break;

            case 'l':/** log **/
                strncpy(gko.opt.logpath, optarg, sizeof(gko.opt.logpath));
                break;

            case '?':
                break;

            case 'v':
                clnt_show_version();
                exit(0);
                break;

            default:
                fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
                break;
        }
    }

    /**
     * process the extra 2 args: source and destination
     **/
    if (argc - optind == 2)
    {
        /** the data src **/
        char * gko_src = argv[optind++];
        char * gko_dst = argv[optind++];
        char * p;
        for (p = gko_src; *p != ':'; p++)
        {
            ;
        }
        *p = '\0';
        if (p - gko_src < 2)
        {
            fprintf(stderr, "Invalid src server\n");
            exit(1);
        }
        struct sockaddr_in src_addr_in;
        in_addr_t serv;
        int addr_len;
        addr_len = getaddr_my(gko_src, &serv);
        if (!addr_len)
        {
            fprintf(stderr, "Can't resolve src server\n");
            exit(1);
        }
        memcpy(&src_addr_in.sin_addr.s_addr, &serv, addr_len);
        strncpy(gko.the_serv.addr, inet_ntoa(src_addr_in.sin_addr),
                sizeof(gko.the_serv.addr));
        strncpy(jo->uri, p + 1, sizeof(jo->uri));
        inplace_strip_tailing_slash(jo->uri);

        /** the data dst **/
        strncpy(jo->path, gko_dst, sizeof(jo->path));
        //inplace_strip_tailing_slash(jo->path);
        fprintf(stderr, "serv: %s:%d, uri: %s, dest: %s\n", gko.the_serv.addr,
                gko.the_serv.port, jo->uri, jo->path);
    }
    else
    {
        if (argc - optind > 2)
        {
            fprintf(stderr, "Too many args\n");
        }
        else
        {
            fprintf(stderr, "Too few args\n");
        }
        clnt_show_help();
        exit(1);
    }

    return 0;
}

/**
 * @brief process args for server
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int serv_parse_opt(int argc, char *argv[])
{

    struct option long_options[] =
        {
            { "help", no_argument, 0, 'h' },
            { "uplimit", required_argument, 0, 'u' },
            { "bind", required_argument, 0, 'b' },
            { "port", required_argument, 0, 'p' },
            { "log", required_argument, 0, 'l' },
            { "worker_thread", required_argument, 0, 't' },
            { "connlimit", required_argument, 0, 'n' },
//            { "seedspeed", required_argument, 0, 'e' },
            { "version", no_argument, 0, 'v' },
            { 0, 0, 0, 0 } };

    /**
     * set default up down rate limit
     **/
    gko.opt.limit_up_rate = SERV_LIMIT_UP_RATE;
    gko.opt.limit_down_rate = 0; /// means no limit
    gko.opt.limit_mk_seed_rate = LIMIT_MK_SEED_RATE;
    gko.opt.port = SERV_PORT;
    gko.opt.worker_thread = SERV_ASYNC_THREAD_NUM;
    gko.opt.connlimit = SERV_POOL_SIZE;
    gko.opt.bind_ip = htons(INADDR_ANY);
    strncpy(gko.opt.logpath, SERVER_LOG, sizeof(gko.opt.logpath));

    /** process args **/
    while (1)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "hu:b:p:l:t:n:e:v", long_options,
                &option_index);
        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 0: /** if 3rd field of struct option is non zero **/
                fprintf(stderr, "option %s with arg %s\n",
                        long_options[option_index].name, optarg ? optarg : " ");
                break;

            case 'h':
                gko.opt.need_help = 1;
                serv_show_help();
                exit(0);
                break;

            case 'u':/** uplimit **/
                gko.opt.limit_up_rate = strtoimax(optarg, NULL, 10) * 1024 * 1024;
                if (gko.opt.limit_up_rate <= 0 || gko.opt.limit_up_rate > 2047 * 1024
                        * 1024)
                {
                    fprintf(stderr, "Invalid uplimit num: %d\n",
                            gko.opt.limit_up_rate);
                    exit(1);
                }
                fprintf(stderr, "gko.opt.limit_up_rate: %d\n", gko.opt.limit_up_rate);
                break;

            case 'b':/** bind ip **/
                in_addr_t serv;
                int addr_len;

                addr_len = getaddr_my(optarg, &serv);
                if (!addr_len)
                {
                    fprintf(stderr, "Can't resolve src server\n");
                    exit(1);
                }
                memcpy(&gko.opt.bind_ip, &serv, addr_len);
                break;

            case 'p':/** port **/
                gko.opt.port = strtoimax(optarg, NULL, 10);
                if (gko.opt.port < MIN_PORT || gko.opt.port > MAX_PORT)
                {
                    fprintf(stderr, "Invalid port num: %d\n", gko.opt.port);
                    exit(1);
                }
                break;

            case 'l':/** log **/
                strncpy(gko.opt.logpath, optarg, sizeof(gko.opt.logpath));
                break;

            case 't':/** worker_thread **/
                gko.opt.worker_thread = strtoimax(optarg, NULL, 10);
                if (gko.opt.worker_thread < 1 || gko.opt.worker_thread > 2047
                        * 1024 * 1024)
                {
                    fprintf(stderr, "Invalid worker_thread num: %d\n",
                            gko.opt.worker_thread);
                    exit(1);
                }
                break;

            case 'n':/** connlimit **/
                gko.opt.connlimit = strtoimax(optarg, NULL, 10);
                if (gko.opt.connlimit < 1 || gko.opt.connlimit > 2047 * 1024
                        * 1024)
                {
                    fprintf(stderr, "Invalid connlimit num: %d\n",
                            gko.opt.connlimit);
                    exit(1);
                }
                break;

//            case 'e':/** seedspeed **/
//                gko.opt.limit_mk_seed_rate = strtoimax(optarg, NULL, 10) * 1024
//                        * 1024;
//                if (gko.opt.limit_mk_seed_rate < 1024 * 1024
//                        || gko.opt.limit_mk_seed_rate > 2047 * 1024 * 1024)
//                {
//                    fprintf(stderr, "Invalid limit_mk_seed_rate num: %d\n",
//                            gko.opt.limit_mk_seed_rate);
//                    exit(1);
//                }
//                break;

            case '?':
                break;

            case 'v':
                serv_show_version();
                exit(0);
                break;

            default:
                fprintf(stderr, "?? getopt returned character code 0%o ??\n", c);
                break;
        }
    }

    /** if args remained, then ... **/
    {
        if (argc - optind > 0)
        {
            fprintf(stderr, "Too many args: %d\n", argc - optind);
            serv_show_help();
            exit(1);
        }
    }

    return 0;
}
