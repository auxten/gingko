/*
 *  gingko_common.h
 *  gingko
 *
 *  Created by Auxten on 11-4-20.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */
//#define GINGKO_SERV

#ifndef GINGKO_COMMON_H_
#define GINGKO_COMMON_H_

/************** FUNC DICT **************/
char cmd_list[CMD_COUNT][8] = {
                                 { "HELO" },
                                 { "JOIN" },
                                 { "GETT" },
                                 { "NEWW" },
                                 { "QUIT" },
                                 { "NONE" } };
//server func list
func_t func_list_s[CMD_COUNT] = {
                                  helo_serv_s,
                                  join_job_s,
                                  get_blocks_s,
                                  new_host_s,
                                  quit_job_s,
                                  g_none_s };

/************** FUNC DICT **************/

/*
 * event handle of sendfile
 */
void ev_fn_gsendfile(int fd, short ev, void *arg) {
    s_gsendfile_arg * a = (s_gsendfile_arg *) arg;
    off_t tmp_off = a->offset + a->send_counter;
    u_int64_t tmp_counter = a->count - a->send_counter;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 1;
    if (((a->sent = gsendfile(fd, a->in_fd, &tmp_off, &tmp_counter)) >= 0)) {
        //printf("res: %d\n",res);
        //printf("%s",client->read_buffer+read_counter);
#ifdef GINGKO_CLNT
            bw_up_limit(a->sent);
#endif
        //printf("sentfile: %d\n", a->sent);
        if ((a->send_counter += a->sent) == a->count) {
            event_del(&(a->ev_write));
        }
        a->retry = 0;
    }else{
        perror("gsendfile error");
        if(a->retry++ > 3) {
            event_base_loopexit(a->ev_base, &t);
        }
    }
    return;
}

/*
 * handle for server replying GET
 */
void * get_blocks_s(void * uri, int fd) {
    int64_t i, block_have;
    char msg[MSG_LEN];
    char * arg_array[4];
    char * c_uri = (char *) uri;
    sep_arg(c_uri, arg_array, 4);
    //    for (i = 0; i < 4; i++) {
    //        printf("%s\n", arg_array[i]);
    //    }
    //    printf("##########get_blocks: %s\n", req);
    int64_t start = atol(arg_array[2]);
    int64_t count = atol(arg_array[3]);
#ifdef GINGKO_SERV
    extern map<string, s_job> m_jobs;
    char ready_to_serv = 1;
    string uri_string(arg_array[1]);
    pthread_rwlock_rdlock(&grand_lock);
    if(m_jobs.find(uri_string) == m_jobs.end()) {
        perr("got non exist get uri %s\n", uri_string.c_str());
        return (void *) 0;
    }
    s_job *jo = &(m_jobs[uri_string]);
    pthread_rwlock_unlock(&grand_lock);
#else
    extern char ready_to_serv; // client upload ready flag
    extern s_job job;
    s_job *jo = &job;
#endif /* GINGKO_SERV */
    //find the block i can send
    //printf("done flag of %d: %d\n", 0, (jo->blocks+0)->done);
    if (ready_to_serv) {
        for (i = 0; i < count; i++) {
            if (!(jo->blocks + ((start + i) % jo->block_count))->done)
                break;
        }
    } else {
        i = 0;
    }
    block_have = i;
    snprintf(msg, MSG_LEN, "HAVE\t%lld", block_have);
    if ((i = sendall(fd, msg, MSG_LEN, 0)) < 0) {
        printf("sent: %lld\n", i);
        perror("sending HAVE error!");
    }
    if ((i = sendblocks(fd, jo, start, block_have)) < 0) {
        printf("sendblocks: %lld\n", i);
        perror("sendblocks error!");
    }
    return (void *) 0;
}

/*
 * reply HELO req
 */
void * helo_serv_s(void * uri, int fd) {
    int i;
    if ((i = sendall(fd, "HI", 2, 0)) < 0) {
        printf("sent: %d\n", i);
        perror("sending HI error!");
    }
    return (void *) 0;
}

/*
 * reply JOIN req
 */
void * join_job_s(void * uri, int fd) {
    int i;
#ifdef GINGKO_SERV
    extern map<string, s_job> m_jobs;
    s_job sjob, *p;
    s_host h;
    memset(&h, 0, sizeof(h));
    char * arg_array[4];
    char * c_uri = (char *) uri;
    s_host * host_array;
    map<string, s_job>::iterator it;
    set<s_host>::iterator host_it;
    printf("join_job %s\n", c_uri);
    // req fields seperated by \t
    sep_arg(c_uri, arg_array, 4);
    //    for (i = 0; i < 4; i++) {
    //        printf("%s\n", arg_array[i]);
    //    }
    string uri_string((char *) (arg_array[1]));
    printf("%s\n", uri_string.c_str());

    strncpy(h.addr, arg_array[2], IP_LEN);
    h.port = atoi(arg_array[3]);
    //pthread lock
    //pthread_mutex_lock(&mutex_join);
    pthread_rwlock_wrlock(&grand_lock);
    it = m_jobs.find(uri_string);
    if (it != m_jobs.end()) //not first host of the job
    {
        p = &(it->second);
        pthread_rwlock_unlock(&grand_lock);
        pthread_rwlock_wrlock(&job_lock[p->lock_id].lock);
        (*p->host_set).insert(h);
        p->host_num = (*p->host_set).size();
        pthread_rwlock_unlock(&job_lock[p->lock_id].lock);
    } else { //first host of the job, create the job
        //find an available job specific lock
        for (i = 0; i < MAX_JOBS; i++) {
            if (job_lock[i].state == LK_FREE) {
                break;
            }
        }
        job_lock[i].state = LK_USING;
        sjob.lock_id = i;
        m_jobs[uri_string] = sjob;
        p = &(m_jobs[uri_string]);
        pthread_rwlock_unlock(&grand_lock);
        pthread_rwlock_wrlock(&job_lock[sjob.lock_id].lock);
        strncpy(p->uri, arg_array[1], MAX_URI);
        strncpy(p->path, arg_array[1], MAX_PATH_LEN);
        p->host_set = new set<s_host>;
        (*p->host_set).insert(h);
        p->host_num = (*p->host_set).size();
        recurse_dir(p);
        pthread_rwlock_unlock(&job_lock[sjob.lock_id].lock);
    }
    //pthread unlock
    //pthread_mutex_unlock(&mutex_join);
    //    for (i = 0; i < 16; i++)
    //        printf("%02x", (p->blocks)->md5[i]);
    //    printf("\n");

    //reply client with s_job
    if ((i = sendall(fd, (const void *) p, sizeof(s_job), 0)) < 0) {
        printf("sent: %d\n", i);
        perror("sending s_job error!");
    }
    if (p->file_count) {
        //send the s_file
        if ((i = sendall(fd, (const void *) (p->files), p->files_size, 0)) < 0) {
            perror("sending s_file error!");
        }
    }
    if (p->block_count) {
        //send the s_block
        if ((i = sendall(fd, (const void *) (p->blocks), p->blocks_size, 0))
                < 0) {
            perror("sending s_blocks error!");
        }
    }
    /*
     * send known host, calloc 1 item larger to indicate the end of array
     */
    host_array = (s_host *) calloc(p->host_num + 1, sizeof(s_host));
    if(! host_array) {
        perr("calloc host_array failed %s\n", strerror(errno));
    }
    //copy the set to array So called "Serialize"
    pthread_rwlock_wrlock(&job_lock[p->lock_id].lock);
    perror("^^^^^^^^^^before copy");
    copy((*(p->host_set)).begin(), (*(p->host_set)).end(), host_array);
    perror("^^^^^^^^^^after copy");
    pthread_rwlock_unlock(&job_lock[p->lock_id].lock);
    if ((i = sendall(fd, (const void *) (host_array),
                            p->host_num * sizeof(s_host), 0)) < 0) {
        perror("sending host_set error!");
    }
    broadcast_join(host_array, &h);
//    perror("^^^^^^^^^^before free");
    free(host_array);
//    perror("^^^^^^^^^^after free");
#else
    extern s_job job;
    memset(&job, 0, sizeof(s_job));
    if ((i = sendall(fd, (const void *) (&job), sizeof(s_job), 0)) < 0) {
        printf("sent: %d\n", i);
        perror("sending s_job error!");
    }
#endif /* GINGKO_SERV */
    return (void *) 0;
}

/*
 * clnt,serv handle
 * "QUIT\t%s\t%s\t%d", job.uri, quit_host->addr, quit_host->port
 */
void * quit_job_s(void * uri, int fd) {
#ifdef GINGKO_SERV
    extern map<string, s_job> m_jobs;
    s_job *p;
    s_host h;
    memset(&h, 0, sizeof(h));
    char * arg_array[4];
    char * c_uri = (char *) uri;
    map<string, s_job>::iterator it;
    printf("quit_job %s\n", c_uri);
    // req fields seperated by \t
    sep_arg(c_uri, arg_array, 4);
    //    for (i = 0; i < 4; i++) {
    //        printf("%s\n", arg_array[i]);
    //    }
    string uri_string((char *) (arg_array[1]));
    printf("%s\n", uri_string.c_str());

    strncpy(h.addr, arg_array[2], IP_LEN);
    h.port = atoi(arg_array[3]);
    //pthread lock
    //pthread_mutex_lock(&mutex_join);
    pthread_rwlock_wrlock(&grand_lock);
    it = m_jobs.find(uri_string);
    if (it != m_jobs.end()) { //found the job
        p = &(it->second);
        pthread_rwlock_unlock(&grand_lock);
        pthread_rwlock_wrlock(&job_lock[p->lock_id].lock);
        (*p->host_set).erase(h);
        p->host_num = (*p->host_set).size();
        s_host * host_array = (s_host *) calloc(p->host_num + 1, sizeof(s_host));
        if(! host_array) {
            perr("calloc host_array failed %s\n", strerror(errno));
        }
        copy((*(p->host_set)).begin(), (*(p->host_set)).end(), host_array);
        pthread_rwlock_unlock(&job_lock[p->lock_id].lock);
        printf("job: %s, host_num: %d\n", p->uri, p->host_num);
        if(p->host_num == 0) {
            /*
             * if the host_set is empty, del the job things
             */
            erase_job(uri_string);
        } else {
            /*
             * broadcast QUIT
             */
            s_host * p_h = host_array;
            char buf[SHORT_MSG_LEN] = {'\0'};
            sprintf(buf, "QUIT\t%s\t%d", h.addr, h.port);
            while (p_h->port) {
                sendcmd(p_h, buf, 2, 2);
                p_h++;
            }
        }
        free(host_array);
    } else { //find no job
        pthread_rwlock_unlock(&grand_lock);
        perr("find no job: %s", uri_string.c_str());
    }
#else
    printf("quit_job\n");
#endif /* GINGKO_SERV */
    return (void *) 0;
}

/*
 * clnt handle "NEWW\t%s\t%d", h->addr, h->port
 */
void * new_host_s(void * uri, int fd) {
#ifdef GINGKO_SERV
    perr("server recv NEWW cmd\n");
#else
    extern char ready_to_serv; // client upload ready flag
    extern s_job job;
    extern vector<s_host> hosts_noready;
    int64_t i;
    char * arg_array[3];
    s_host h;
    char * c_uri = (char *) uri;
    printf("ready_to_serv: %d %s\n", ready_to_serv, c_uri);
    sep_arg(c_uri, arg_array, 3);
    for (i = 0; i < 3; i++) {
        printf("%s\n", arg_array[i]);
    }
    memset(&h, 0, sizeof(s_host));
    strncpy(h.addr, arg_array[1], IP_LEN);
    h.port = atoi(arg_array[2]);
    if (ready_to_serv) {
        /*
         * when ready_to_serv
         * insert and host_hash the host
         */
        pthread_rwlock_wrlock(&clnt_lock);
        (*(job.host_set)).insert(h);
        pthread_rwlock_unlock(&clnt_lock);
        host_hash(&job, &h, NULL);

    } else {
        pthread_mutex_lock(&noready_mutex);
        hosts_noready.push_back(h);
        pthread_mutex_unlock(&noready_mutex);
        fprintf(stderr, "NOT ready to serv\n");
    }
#endif /* GINGKO_SERV */
    return (void *) 0;
}

#endif /* GINGKO_COMMON_H_ */
