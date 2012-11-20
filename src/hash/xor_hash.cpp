/**
 * xor_hash.cpp
 *
 *  Created on: 2011-5-9
 *      Author: auxten
 **/
#include <pthread.h>
#include "xor_hash.h"
#include "../gingko.h"
#include "../log.h"
#include "../job_state.h"
#include "../limit.h"

/**
 * @brief xor hash a given length buf
 *
 * @see
 * @note
 *     if hval is not 0, use it as the init hash value
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash(const void *key, int len, unsigned hval)
{
#if defined(ROT_XOR_HASH)
    u_char *p = (u_char *) key;
    hval = hval ? hval : 2166136261;
#if defined(HASH_BYTE_NUM_ONCE)
    for (int i = 0; i <= len - HASH_BYTE_NUM_ONCE; i += HASH_BYTE_NUM_ONCE)
    {
        hval = ROLL(hval) ^ p[i];
        hval = ROLL(hval) ^ p[i + 1];
        hval = ROLL(hval) ^ p[i + 2];
        hval = ROLL(hval) ^ p[i + 3];
#if HASH_BYTE_NUM_ONCE == 8
        hval = ROLL(hval) ^ p[i + 4];
        hval = ROLL(hval) ^ p[i + 5];
        hval = ROLL(hval) ^ p[i + 6];
        hval = ROLL(hval) ^ p[i + 7];
#endif /** HASH_BYTE_NUM_ONCE == 8 **/
    }
    /**
     * hash the remained bytes
     **/
    for (int i = len - len % HASH_BYTE_NUM_ONCE; i < len; i++)
    {
        hval = ROLL(hval) ^ p[i];
    }
#else
    for (int i = 0; i < len; i++)
    {
        hval = ROLL(hval) ^ p[i];
    }
#endif

#elif defined(FNV_XOR_HASH)
    u_char *p = (u_char *) key;
    hval = hval ? hval : 2166136261;

    for (int i = 0; i < len; i++)
    {
#if defined(NO_SO_CALLED_FNV_OPTIMIZE)
        hval = (hval * 16777619) ^ p[i];
#else
        hval += (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval
                << 24);
        hval ^= p[i];
#endif
    }

    return hval;
#endif /** ROT_XOR_HASH **/
    return hval;
}

/**
 * @brief check if the fnv check sum is OK
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char digest_ok(void * buf, s_block_t * b)
{
    return (xor_hash(buf, b->size, 0) == b->digest);
}

/**
 * @brief xor hash specified block
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash_block(s_job_t * jo, GKO_INT64 block_id, u_char * buf)
{
    s_file_t * files = jo->files;
    s_block_t * blocks = jo->blocks;
    GKO_INT64 read_counter = 0;
    GKO_INT64 file_i = (blocks + block_id)->start_f;
    GKO_INT64 offset = 0;
    int fd;
    unsigned tmp_hash = 0;

    if (FAIL_CHECK(-1 == (fd = open((files + ((blocks + block_id)->start_f))->name,
                            O_RDONLY | O_NOFOLLOW))))
    {
        gko_log(WARNING, "file open() error!");
    }
    memset(buf, 0, BLOCK_SIZE);
    offset = (blocks + block_id)->start_off;
    while (read_counter < (blocks + block_id)->size)
    {
        GKO_INT64 tmp = pread(fd, buf + read_counter,
                (blocks + block_id)->size - read_counter, offset);
        if (FAIL_CHECK(tmp < 0))
        {
            gko_log(WARNING, "pread failed");
        }
        if (LIKELY(tmp))
        {
            ///printf("read: %ld\n", tmp);
            tmp_hash = xor_hash(buf + read_counter, (int) tmp, tmp_hash);
            read_counter += tmp;
            offset += tmp;
        }
        else
        {
            close(fd);
            ///if the next if a nonfile then next
            file_i = next_f(jo, file_i);
            if (FAIL_CHECK(-1
                    == (fd = open(
                                    (files + ((blocks + block_id)->start_f)
                                            + file_i)->name, O_RDONLY | O_NOFOLLOW))))
            {
                fprintf(stderr, "filename: %s\n",
                        (files + ((blocks + block_id)->start_f) + file_i)->name);
                gko_log(WARNING, "filename: %s",
                        (files + ((blocks + block_id)->start_f) + file_i)->name);
            }
            offset = 0;

        }
    }
    (blocks + block_id)->digest = tmp_hash;
    ///    printf("buf: %d\n", sizeof(buf));
    ///    memset(buf, 0, sizeof(buf));
    ///    printf("buf: %d\n", sizeof(buf));
    close(fd);
    return tmp_hash;
}

/**
 * @brief xor hash the file given
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned xor_hash_file(unsigned value, FILE * fd, off_t * off, size_t * count,
        u_char * buf)
{
    fseeko(fd, *off, SEEK_SET);
    if (FAIL_CHECK(*count != fread(buf, sizeof(char), *count, fd)))
    {
        gko_log(FATAL, "fread error");
    }
    ///fprintf(stderr, "#######################buf: %s\n", buf);
    return xor_hash(buf, *count, value);
}

/**
 * @brief xor hash pthread worker
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void * xor_hash_worker_f(void * a)
{
    /** enable the cancel and make the thread canceled immediately **/
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    hash_worker_thread_arg * arg = (hash_worker_thread_arg *) a;
    int t_idx = arg->index;
    s_job_t * jo = arg->p;
    GKO_INT64 start = arg->range[0];
    GKO_INT64 num = arg->range[1] - arg->range[0];
    gko_log(NOTICE, "range: %lld %lld", arg->range[0], arg->range[1]);
    u_char * buf = (jo->hash_buf)[t_idx];
    if (num == 0)
    {
        delete [] buf;
        (jo->hash_buf)[t_idx] = NULL;
        pthread_exit((void *) 0);
    }
    /**
     * if include the last block, the total size
     *    may be smaller than BLOCK_SIZE * num
     **/
    GKO_INT64 this_time_sent = 0;
    GKO_INT64 total = BLOCK_SIZE * (num - 1)
            + (start + num >= jo->block_count ? (jo->blocks + jo->block_count
                    - 1)->size : BLOCK_SIZE);
    GKO_INT64 b = start;
    GKO_INT64 f = (jo->blocks + start)->start_f;
    size_t block_left = (jo->blocks + start)->size;
    GKO_INT64 file_size;
    FILE * fd = (FILE *) -1;
    off_t offset = (jo->blocks + start)->start_off;
    size_t file_left;

    while (total > 0)
    {
        if (fd == (FILE *) -1)
        {
            fd = fopen((jo->files + f)->name, "r");
        }
        if (FAIL_CHECK(fd <= NULL))
        {
            gko_log(WARNING, "xor_hash_worker_f open error");
            jo->job_state = JOB_FILE_OPEN_ERR;
            delete [] buf;
            (jo->hash_buf)[t_idx] = NULL;
            pthread_exit((void *) -2);
        }
        file_size = (jo->files + f)->size;
        file_left = file_size - offset;

        /** read'n'hash the file **/
        if (LIKELY(block_left < file_left))
        {
            /**block_left < file_left**/
            (jo->blocks + b)->digest = xor_hash_file((jo->blocks + b)->digest,
                    fd, &offset, &block_left, buf);
            b = next_b(jo, b);
            total -= block_left;
            (jo->hash_progress)[t_idx] += block_left;
            offset += block_left;
            this_time_sent = block_left;
            block_left = (jo->blocks + b)->size;
        }
        else
        {
            (jo->blocks + b)->digest = xor_hash_file((jo->blocks + b)->digest,
                    fd, &offset, &file_left, buf);
            fclose(fd);
            fd = (FILE *) -1;
            offset = 0;
            total -= file_left;
            (jo->hash_progress)[t_idx] += file_left;
            this_time_sent = file_left;
            f = next_f(jo, f);
            if (LIKELY(block_left > file_left))
            {
                /**block_left > file_left**/
                block_left -= file_left;
            }
            else
            {
                /**block_left == file_left**/
                b = next_b(jo, b);
                block_left = (jo->blocks + b)->size;
            }
        }
        //mk_seed_limit(this_time_sent, gko.opt.limit_mk_seed_rate);
///        fprintf(stderr, "p_porgress %lld\n", *(arg->p_porgress));
    }

    delete [] buf;
    (jo->hash_buf)[t_idx] = NULL;
    if (fd != (FILE *) -1)
    {
        fclose(fd);
        fd = (FILE *) -1;
    }
    gko_log(TRACE, "xor_hash_worker_f returned successfully");
    pthread_exit((void *) 0);
}
//
//void * xor_hash_worker(void * a)
//{
//    hash_worker_thread_arg * arg = (hash_worker_thread_arg *) a;
//    for (GKO_INT64 i = arg->range[0]; i < arg->range[1]; i++)
//    {
//        xor_hash_block(arg->p, i, arg->buf);
//    }
//
//    pthread_exit((void *) 0);
//}

/**
 * @brief xor hash all blocks for a job given
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int xor_hash_all(s_job_t * jo, hash_worker_thread_arg arg[])
{
    pthread_attr_t attr;
    ///mutex for up bandwidth limit

    /**
     * if size == 13 XOR_HASH_TNUM == 4;range will be 0~3 3~6 6~9 9~13
     **/
    for (int i = 0; i < XOR_HASH_TNUM; i++)
    {
    	(jo->hash_buf)[i] = new u_char[BLOCK_SIZE + 1];
        if (FAIL_CHECK(!(jo->hash_buf)[i]))
        {
            gko_log(FATAL, "new fuv hash buf failed");
            return -1;
        }
        memset((jo->hash_buf)[i], 0, BLOCK_SIZE + 1);
        arg[i].index = i;
        arg[i].range[0] = jo->block_count / XOR_HASH_TNUM * i;
        arg[i].range[1] = jo->block_count / XOR_HASH_TNUM * (i + 1);
        arg[i].p = jo;
        (jo->hash_progress)[i] = 0;
    }
    /** last thread must make the work done **/
    arg[XOR_HASH_TNUM - 1].range[1] = jo->block_count;

    /** initialize the pthread attrib for every worker **/
    pthread_attr_init(&attr);
    /** set attr state for join() in the mother **/
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, MYSTACKSIZE);

    for (int i = 0; i < XOR_HASH_TNUM; i++)
    {
        pthread_create(&jo->hash_worker[i], &attr, xor_hash_worker_f, &(arg[i]));
    }
    /// pthread_attr_* moved to main();

    return 0;
}
