/**
 *  gingko_common.h
 *  gingko
 *
 *  Created by Auxten on 11-4-20.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/
//#define GINGKO_SERV /// for debug
#ifndef GINGKO_COMMON_H_
#define GINGKO_COMMON_H_

extern s_gingko_global_t gko;

/**
 * @brief server func
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * helo_serv_s(void *, int);
GKO_STATIC_FUNC void * join_job_s(void *, int);
GKO_STATIC_FUNC void * quit_job_s(void *, int);
GKO_STATIC_FUNC void * dead_host_s(void *, int);
GKO_STATIC_FUNC void * get_blocks_s(void *, int);
GKO_STATIC_FUNC void * g_none_s(void *, int);
GKO_STATIC_FUNC void * new_host_s(void *, int);
GKO_STATIC_FUNC void * del_host_s(void *, int);
GKO_STATIC_FUNC void * erase_job_s(void *, int);

/**
 * @brief ************** FUNC DICT **************
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static char g_cmd_list[][8] =
        {
                {
                    "GETT" },
                {
                    "NEWW" },
                {
                    "DELE" },
                {
                    "DEAD" },
                {
                    "HELO" },
                {
                    "JOIN" },
                {
                    "QUIT" },
                {
                    "ERSE" },
                {
                    "NONE" } };
/**
 * @brief server func list
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
static func_t g_func_list_s[] =
        {
            get_blocks_s,
            new_host_s,
            del_host_s,
            dead_host_s,
            helo_serv_s,
            join_job_s,
            quit_job_s,
            erase_job_s,
            g_none_s };



/**
 * @brief handle for server replying GET
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * get_blocks_s(void * uri, int fd)
{
    GKO_INT64 i;
    GKO_INT64 block_have;
    char msg[MSG_LEN] = {'\0'};
    char * arg_array[4];
    char * c_uri = (char *) uri;
    gko_log(DEBUG, "##########get_blocks: %s", c_uri);
    if (sep_arg(c_uri, arg_array, 4) != 4)
    {
        gko_log(WARNING, "Wrong GETT cmd: %s", c_uri);
        return ((void *) -1);
    }
    ///    for (i = 0; i < 4; i++) {
    ///        gko_log(NOTICE, "%s", arg_array[i]);
    ///    }
    GKO_INT64 start = atol(arg_array[2]);
    GKO_INT64 count = atol(arg_array[3]);
#ifdef GINGKO_SERV
    extern std::map<std::string, s_job_t *> g_m_jobs;
    std::string uri_string(arg_array[1]);
    pthread_mutex_lock(&g_grand_lock);
    if (g_m_jobs.find(uri_string) == g_m_jobs.end())
    {
        gko_log(WARNING, "got non existed get uri %s", uri_string.c_str());
        pthread_mutex_unlock(&g_grand_lock);
        return (void *) 0;
    }
    s_job_t *jo = g_m_jobs[uri_string];
    pthread_mutex_unlock(&g_grand_lock);
#else
    extern s_job_t g_job;
    s_job_t *jo = &g_job;
#endif /** GINGKO_SERV **/
    ///find the block i can send
    ///printf("done flag of %d: %d", 0, (jo->blocks+0)->done);
#ifdef GINGKO_SERV
    pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
#endif /** GINGKO_SERV **/
    if(jo->job_state == JOB_TO_BE_ERASED)
    {
#ifdef GINGKO_SERV
        pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
#endif /** GINGKO_SERV **/
        return (void *) -1;
    }
    if (gko.ready_to_serv)
    {
        for (i = 0; i < count; i++)
        {
            if (!(jo->blocks + ((start + i) % jo->block_count))->done)
            {
                break;
            }
        }
    }
    else
    {
        i = 0;
    }
#ifdef GINGKO_SERV
    pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
#endif /** GINGKO_SERV **/
    block_have = i;
    snprintf(msg, MSG_LEN, "HAVE\t%lld", block_have);
    gko_log(DEBUG, "%s", msg);
    if ((i = sendall(fd, msg, MSG_LEN, SNDSERV_HAVE_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending HAVE error!");
        return (void *) -1;
    }
    if ((i = sendblocks(fd, jo, start, block_have)) < 0)
    {
        gko_log(DEBUG, "sendblocks error!");
        return (void *) -1;
    }
    return (void *) 0;
}

/**
 * @brief reply HELO req
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * helo_serv_s(void * uri, int fd)
{
    int i;
    if ((i = sendall(fd, "HI", 2, SND_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending HI error!");
    }
    return (void *) 0;
}

/**
 * @brief reply JOIN req
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * join_job_s(void * uri, int fd)
{
    int i;
#ifdef GINGKO_SERV
    extern std::map<std::string, s_job_t *> g_m_jobs;
    s_job_t *jo;
    s_host_t h;
    memset(&h, 0, sizeof(h));
    char * arg_array[4];
    char * c_uri = (char *) uri;
    s_host_t * host_array;
    std::map<std::string, s_job_t *>::iterator it;
    std::set<s_host_t>::iterator host_it;
    gko_log(NOTICE, "join_job %s", c_uri);
    /// req fields seperated by \t
    if (sep_arg(c_uri, arg_array, 4) != 4)
    {
        gko_log(WARNING, "Wrong JOIN cmd: %s", c_uri);
        return (void *) -1;
    }
    std::string uri_string((char *) (arg_array[1]));
    gko_log(NOTICE, "%s", uri_string.c_str());

    strncpy(h.addr, arg_array[2], IP_LEN);
    h.port = atoi(arg_array[3]);
    pthread_mutex_lock(&g_grand_lock);
    it = g_m_jobs.find(uri_string);
    if (it != g_m_jobs.end())
    { ///not first host of the g_job
        jo = it->second;
        pthread_mutex_unlock(&g_grand_lock);
    }
    else
    { ///first host of the g_job, create the g_job
      ///find an available g_job specific lock
        for (i = 0; i < MAX_JOBS; i++)
        {
            if (g_job_lock[i].state == LK_FREE)
            {
                break;
            }
        }
        g_job_lock[i].state = LK_USING;
        jo = new s_job_t;
        if(!jo)
        {
            gko_log(FATAL, "new s_job_t failed");
            pthread_mutex_unlock(&g_grand_lock);
            return (void *) -1;
        }
        memset(jo, 0, sizeof(s_job_t));
        jo->lock_id = i;
        g_m_jobs[uri_string] = jo;

        pthread_mutex_unlock(&g_grand_lock);

        pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
        strncpy(jo->uri, arg_array[1], MAX_URI);
        strncpy(jo->path, arg_array[1], MAX_PATH_LEN);
        jo->host_set = new std::set<s_host_t>;
        (*jo->host_set).insert(h);
        jo->host_num = (*jo->host_set).size();
        gko_log(NOTICE, "new host join, host_num: %d", jo->host_num);
        if (recurse_dir(jo))
        {
            gko_log(FATAL, "recusre dir failed");
            jo->file_count = 0;
            jo->block_count = 0;
            jo->total_size = 0;
            jo->job_state = JOB_RECURSE_ERR;
            pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
        }
        jo->job_state = JOB_RECURSE_DONE;
        if (jo->block_count)
        {
            xor_hash_all(jo, jo->arg);
        }
        pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
    }

    GKO_INT64 percent;
    GKO_INT64 progress;
    if (jo->job_state == JOB_RECURSE_DONE)
    {
        progress = array_sum(jo->hash_progress, XOR_HASH_TNUM);
        percent = jo->total_size ? progress * 100 / jo->total_size : 100;
    }
    else if (jo->job_state == JOB_RECURSE_ERR)
    {
        percent = 100;
    }
    else
    {
        percent = 0;
    }
    gko_log(NOTICE, "make seed progress: %lld%%", percent);
    if ((i = sendall(fd, (const void *) &percent, sizeof(percent), SND_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending progress error!");
        return (void *) -1;
    }

    if (percent != 100)
    {
        return (void *) 0;
    }

    pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
    (*jo->host_set).insert(h);
    jo->host_num = (*jo->host_set).size();
    gko_log(NOTICE, "host_num changed, now host_num: %d", jo->host_num);
    pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);

    gko_log(NOTICE, "sending seed to %s:%u", h.addr, h.port);
    ///reply client with s_job_t
    if ((i = sendall(fd, (const void *) jo, sizeof(s_job_t), SNDBLK_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending s_job_t error!");
    }
    if (jo->file_count)
    {
        ///send the s_file_t
        if ((i = sendall(fd, (const void *) (jo->files), jo->files_size, SNDBLK_TIMEOUT)) < 0)
        {
            gko_log(WARNING, "sending s_file_t error!");
        }
    }
    if (jo->block_count)
    {
        ///send the s_block_t
        if ((i = sendall(fd, (const void *) (jo->blocks), jo->blocks_size, SNDBLK_TIMEOUT))
                < 0)
        {
            gko_log(WARNING, "sending s_block_t error!");
        }
    }

    /**
     * send known host
     **/

    int tmp_host_num = jo->host_num;
    host_array = new s_host_t[tmp_host_num];
    if (!host_array)
    {
        gko_log(FATAL, "new host_array failed");
        return (void *) -1;
    }
    memset(host_array, 0, sizeof(s_host_t) * tmp_host_num);
    ///copy the set to array So called "Serialize"
    int i_a = 0;
    for (std::set<s_host_t>::iterator i = (*(jo->host_set)).begin(); i
            != (*(jo->host_set)).end(); i++)
    {
        memcpy(host_array + i_a, &(*i), sizeof(s_host_t));
        if(++i_a >= tmp_host_num)
        {
            break;
        }
    }
    //copy((*(jo->host_set)).begin(), (*(jo->host_set)).begin() + jo->host_num, host_array);
    if ((i = sendall(fd, (const void *) (host_array),
            tmp_host_num * sizeof(s_host_t), SNDBLK_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending host_set error!");
    }

    delete [] host_array;
    host_array = NULL;


#else
    extern s_job_t g_job;
    memset(&g_job, 0, sizeof(s_job_t));
    if ((i = sendall(fd, (const void *) (&g_job), sizeof(s_job_t), SNDBLK_TIMEOUT)) < 0)
    {
        gko_log(WARNING, "sending s_job_t error!");
    }
#endif /** GINGKO_SERV **/
    return (void *) 0;
}

/**
 * @brief clnt handle "NEWW\t%s\t%d", h->addr, h->port
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * new_host_s(void * uri, int fd)
{
#ifdef GINGKO_SERV
    gko_log(WARNING, "server recv NEWW cmd");
#else
    extern s_job_t g_job;
    char * arg_array[3];
    s_host_t h;
    char * c_uri = (char *) uri;
    gko_log(NOTICE, "gko.ready_to_serv: %d %s", gko.ready_to_serv, c_uri);
    if (sep_arg(c_uri, arg_array, 3) != 3)
    {
        gko_log(WARNING, "Wrong NEWW cmd: %s", c_uri);
        return (void *) -1;
    }

    memset(&h, 0, sizeof(s_host_t));
    strncpy(h.addr, arg_array[1], IP_LEN);
    h.port = atoi(arg_array[2]);
    if (gko.ready_to_serv)
    {
        /**
         * when gko.ready_to_serv
         * insert and host_hash the host
         **/
        pthread_mutex_lock(&g_clnt_lock);
        (*(g_job.host_set)).insert(h);
        pthread_mutex_unlock(&g_clnt_lock);
        host_hash(&g_job, &h, NULL, ADD_HOST);
        gko_log(NOTICE, "NEWW host %s:%d joined", h.addr, h.port);

    }
    else
    {
        pthread_mutex_lock(&g_hosts_new_noready_mutex);
        gko.hosts_new_noready.push_back(h);
        pthread_mutex_unlock(&g_hosts_new_noready_mutex);
    }
#endif /** GINGKO_SERV **/
    return (void *) 0;
}

/**
 * @brief serv handle
 * @brief "DEAD\t%s\t%s\t%d", g_job.uri, dead_host->addr, dead_host->port
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * dead_host_s(void * uri, int fd)
{
#ifdef GINGKO_SERV
    extern std::map<std::string, s_job_t *> g_m_jobs;
    s_job_t *jo;
    s_host_t h;
    memset(&h, 0, sizeof(h));
    char * arg_array[4];
    char * c_uri = (char *) uri;
    std::map<std::string, s_job_t *>::iterator it;
    gko_log(NOTICE, "dead_host %s", c_uri);
    /// req fields seperated by \t
    if (sep_arg(c_uri, arg_array, 4) != 4)
    {
        gko_log(WARNING, "Wrong DEAD cmd: %s", c_uri);
        return (void *) -1;
    }

    std::string uri_string((char *) (arg_array[1]));
    gko_log(NOTICE, "%s", uri_string.c_str());

    strncpy(h.addr, arg_array[2], IP_LEN);
    h.port = atoi(arg_array[3]);
    pthread_mutex_lock(&g_grand_lock);
    it = g_m_jobs.find(uri_string);
    if (it != g_m_jobs.end())
    { ///found the g_job
        jo = it->second;
        pthread_mutex_unlock(&g_grand_lock);
        int connect_ret = connect_host(&h, RCV_TIMEOUT, SND_TIMEOUT);
        if (connect_ret == HOST_DOWN_FAIL)
        {
            pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
            (*jo->host_set).erase(h);
            jo->host_num = (*jo->host_set).size();
            s_host_t * host_array = new s_host_t[jo->host_num + 1];
            if (!host_array)
            {
                gko_log(FATAL, "new host_array failed");
                return (void *) -1;
            }
            memset(host_array, 0, sizeof(s_host_t) * (jo->host_num + 1));
            copy((*(jo->host_set)).begin(), (*(jo->host_set)).end(), host_array);
            pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
            gko_log(NOTICE, "g_job: %s, host_num: %d", jo->uri, jo->host_num);
            if (jo->host_num == 0)
            {
                /**
                 * if the host_set is empty, del the g_job things
                 **/
                erase_job(uri_string);
            }
            else
            {
                /**
                 * broadcast DELE
                 **/
                s_host_t * p_h = host_array;
                char buf[SHORT_MSG_LEN] =
                        {
                            '\0' };
                snprintf(buf, SHORT_MSG_LEN, "DELE\t%s\t%u", h.addr, h.port);
                while (p_h->port)
                {
                    sendcmd(p_h, buf, 2, 2);
                    p_h++;
                }
            }

            delete [] host_array;
            host_array = NULL;
        }
        else if(connect_ret >= 0)
        {
            gko_log(NOTICE, "clnt %s:%u not dead", h.addr, h.port);
            close_socket(connect_ret);
        }
    }
    else
    { ///found no g_job
        pthread_mutex_unlock(&g_grand_lock);
        gko_log(WARNING, "find no g_job: %s", uri_string.c_str());
    }
#else
    gko_log(WARNING, "client recv DEAD");
#endif /** GINGKO_SERV **/
    return (void *) 0;
}

/**
 * @brief clnt,serv handle
 * @brief "QUIT\t%s\t%s\t%d", g_job.uri, quit_host->addr, quit_host->port
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * quit_job_s(void * uri, int fd)
{
#ifdef GINGKO_SERV
    extern std::map<std::string, s_job_t *> g_m_jobs;
    s_job_t *jo;
    s_host_t h;
    memset(&h, 0, sizeof(h));
    char * arg_array[4];
    char * c_uri = (char *) uri;
    std::map<std::string, s_job_t *>::iterator it;
    gko_log(NOTICE, "quit_job %s", c_uri);
    /// req fields seperated by \t
    if (sep_arg(c_uri, arg_array, 4) != 4)
    {
        gko_log(WARNING, "Wrong QUIT cmd: %s", c_uri);
        return (void *) -1;
    }

    std::string uri_string((char *) (arg_array[1]));
    gko_log(NOTICE, "%s", uri_string.c_str());

    strncpy(h.addr, arg_array[2], IP_LEN);
    h.port = atoi(arg_array[3]);
    pthread_mutex_lock(&g_grand_lock);
    it = g_m_jobs.find(uri_string);
    if (it != g_m_jobs.end())
    {/** found the g_job **/
        jo = it->second;
        pthread_mutex_unlock(&g_grand_lock);
        pthread_mutex_lock(&g_job_lock[jo->lock_id].lock);
        if ((*jo->host_set).find(h) != (*jo->host_set).end())
        {/** the host is in this g_job host_set **/
            (*jo->host_set).erase(h);
            jo->host_num = (*jo->host_set).size();
            s_host_t * host_array = new s_host_t[jo->host_num + 1];
            if (!host_array)
            {
                gko_log(FATAL, "new host_array failed");
                return (void *) -1;
            }
            memset(host_array, 0, sizeof(s_host_t) * (jo->host_num + 1));
            copy((*(jo->host_set)).begin(), (*(jo->host_set)).end(), host_array);
            pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
            gko_log(NOTICE, "g_job: %s, host_num: %d", jo->uri, jo->host_num);
            if (jo->host_num == 0)
            {
                /**
                 * if the host_set is empty, del the g_job things
                 **/
                erase_job(uri_string);
            }
            else
            {
                /**
                 * broadcast DELE
                 **/
                s_host_t * p_h = host_array;
                char buf[SHORT_MSG_LEN] =
                        {
                            '\0' };
                snprintf(buf, SHORT_MSG_LEN, "DELE\t%s\t%d", h.addr, h.port);
                while (p_h->port)
                {
                    sendcmd(p_h, buf, 2, 2);
                    p_h++;
                }
            }
            delete [] host_array;
            host_array = NULL;
        }
        else
        {
            pthread_mutex_unlock(&g_job_lock[jo->lock_id].lock);
            gko_log(NOTICE, "QUITed host %s:%d is not in this g_job", h.addr,
                    h.port);
        }
    }
    else
    { /** find no g_job **/
        pthread_mutex_unlock(&g_grand_lock);
        gko_log(WARNING, "find no g_job: %s", uri_string.c_str());
    }
#else
    gko_log(WARNING, "client recv QUIT");
#endif /** GINGKO_SERV **/
    return (void *) 0;
}

/**
 * @brief clnt handle "DELE\t%s\t%d", h->addr, h->port
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * del_host_s(void * uri, int fd)
{
#ifdef GINGKO_SERV
    gko_log(WARNING, "server recv DELE cmd");
#else
    extern s_job_t g_job;
    GKO_INT64 i;
    char * arg_array[3];
    s_host_t h;
    char * c_uri = (char *) uri;
    gko_log(NOTICE, "gko.ready_to_serv: %d %s", gko.ready_to_serv, c_uri);
    if (sep_arg(c_uri, arg_array, 3) != 3)
    {
        gko_log(WARNING, "Wrong DELE cmd: %s", c_uri);
        return (void *) -1;
    }
    for (i = 0; i < 3; i++)
    {
        gko_log(NOTICE, "%s", arg_array[i]);
    }
    memset(&h, 0, sizeof(s_host_t));
    strncpy(h.addr, arg_array[1], IP_LEN);
    h.port = atoi(arg_array[2]);
    if (gko.ready_to_serv)
    {
        /**
         * when gko.ready_to_serv del the host
         **/
        pthread_mutex_lock(&g_clnt_lock);
        (*(g_job.host_set)).erase(h);
        pthread_mutex_unlock(&g_clnt_lock);
        host_hash(&g_job, &h, NULL, DEL_HOST);

    }
    else
    {
        pthread_mutex_lock(&g_hosts_del_noready_mutex);
        gko.hosts_del_noready.push_back(h);
        pthread_mutex_unlock(&g_hosts_del_noready_mutex);
    }
#endif /** GINGKO_SERV **/
    return (void *) 0;
}

/**
 * @brief erase the g_job related stuff
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * erase_job_s(void * uri, int fd)
{
#ifdef GINGKO_SERV
    char msg[SHORT_MSG_LEN] =
            {
                '\0' };
    char * arg_array[2];
    char * c_uri = (char *) uri;
    if (sep_arg(c_uri, arg_array, 2) != 2)
    {
        gko_log(WARNING, "Wrong ERSE cmd: %s", c_uri);
        return (void *) -1;
    }

    std::string uri_string(arg_array[1]);
    if (erase_job(uri_string) == 0)
    {
        snprintf(msg, SHORT_MSG_LEN, "ERSE\tSUCCED");
    }
    else
    {
        snprintf(msg, SHORT_MSG_LEN, "NO\tMATCH\tSEED");
    }
    sendall(fd, msg, SHORT_MSG_LEN, SND_TIMEOUT);
#else
    gko_log(WARNING, "got ERSE cmd on client: %s", (char *) uri);
#endif

    return (void *) 0;
}

/**
 * @brief failback handler for request
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC void * g_none_s(void *, int)
{
    gko_log(NOTICE, "none");
    return (void *) 0;
}


#endif /** GINGKO_COMMON_H_ **/
