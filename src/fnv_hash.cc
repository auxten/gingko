/*
 * fnv_hash.cc
 *
 *  Created on: 2011-5-9
 *      Author: auxten
 */
#include <pthread.h>
#include "fnv_hash.h"
#include "gingko.h"

unsigned fnv_hash_block(s_job * jo, long block_id, unsigned char * buf) {
    s_file * files = jo->files;
    s_block * blocks = jo->blocks;
    long read_counter = 0;
    long file_i = (blocks + block_id)->start_f;
    long offset = 0;
    int fd;
    unsigned tmp_hash = 0;

    if (FAIL_CHECK(-1 == (fd = open((files + ((blocks + block_id)->start_f))->name,
            O_RDONLY | O_NOFOLLOW)))) {
        perror("file open() error!");
    }
    memset(buf, 0, BLOCK_SIZE);
    offset = (blocks + block_id)->start_off;
    while (read_counter < (blocks + block_id)->size) {
        long tmp;
        if (FAIL_CHECK(tmp = pread(fd, buf + read_counter,
                (blocks + block_id)->size - read_counter, offset) == -1)) {
            perr("pread failed");
        }
        switch (tmp) {
            case 0:
                close(fd);
                //if the next if a nonfile then next
                file_i = next_f(jo, file_i);
                if (FAIL_CHECK(-1
                        == (fd = open(
                                (files + ((blocks + block_id)->start_f)
                                        + file_i)->name, O_RDONLY | O_NOFOLLOW)))) {
                    fprintf(
                            stderr,
                            "filename: %s\n",
                            (files + ((blocks + block_id)->start_f) + file_i)->name);
                    perr("filename: %s\n",
                            (files + ((blocks + block_id)->start_f) + file_i)->name);
                }
                offset = 0;
                break;

            default:
                //printf("read: %ld\n", tmp);
                tmp_hash = fnv_hash(buf + read_counter, (int) tmp, tmp_hash);
                read_counter += tmp;
                offset += tmp;
                break;
        }
    }
    (blocks + block_id)->digest = tmp_hash;
    //    printf("buf: %d\n", sizeof(buf));
    //    memset(buf, 0, sizeof(buf));
    //    printf("buf: %d\n", sizeof(buf));
    close(fd);
    return tmp_hash;
}

unsigned fnv_hash_file(unsigned value, FILE * fd, off_t * off, size_t * count,
        unsigned char * buf) {
    fseeko(fd, *off, SEEK_SET);
    if (FAIL_CHECK(*count != fread(buf, sizeof(char), *count, fd))) {
        perr("fread error");
    }
    //fprintf(stderr, "#######################buf: %s\n", buf);
    return fnv_hash(buf, *count, value);
}

void * fnv_hash_worker_f(void * a) {
    fnv_thread_arg * arg = (fnv_thread_arg *) a;
    s_job * jo = arg->p;
    long start = arg->range[0];
    long num = arg->range[1] - arg->range[0];
    unsigned char * buf;
    if (FAIL_CHECK((buf = (unsigned char *) calloc(BLOCK_SIZE + 1, 1)) == NULL)) {
        perror("calloc fuv hash buf failed");
    }
    if (num == 0) {
        pthread_exit((void *) 0);
    }
    /*
     * if include the last block, the total size
     *    may be smaller than BLOCK_SIZE * num
     */
    long sent = 0;
    long total = BLOCK_SIZE * (num - 1)
                    + (start + num >= jo->block_count ? (jo->blocks + jo->block_count
                            - 1)->size : BLOCK_SIZE);
    long b = start;
    long f = (jo->blocks + start)->start_f;
    size_t block_left = (jo->blocks + b)->size;
    long file_size;
    FILE * fd = (FILE *) -1;
    off_t offset = (jo->blocks + start)->start_off;
    size_t file_left;
    while (total > 0) {
        if (fd == (FILE *) -1) {
            fd = fopen((jo->files + f)->name, "r");
        }
        if (FAIL_CHECK(fd <= 0)) {
            perror("fnv_hash_worker_f open error");
            pthread_exit((void *) -2);
        }
        file_size = (jo->files + f)->size;
        file_left = file_size - offset;

        if (LIKELY(block_left < file_left)) {
            /*block_left < file_left*/
            (jo->blocks + b)->digest = fnv_hash_file((jo->blocks + b)->digest,
                    fd, &offset, &block_left, buf);
            b = next_b(jo, b);
            total -= block_left;
            offset += block_left;
            sent += block_left;
            block_left = (jo->blocks + b)->size;
        } else {
            (jo->blocks + b)->digest = fnv_hash_file((jo->blocks + b)->digest,
                    fd, &offset, &file_left, buf);
            fclose(fd);
            fd = (FILE *) -1;
            offset = 0;
            total -= file_left;
            sent += file_left;
            f = next_f(jo, f);
            if (LIKELY(block_left > file_left)) {
                /*block_left > file_left*/
                block_left -= file_left;
            } else {
                /*block_left == file_left*/
                b = next_b(jo, b);
                block_left = (jo->blocks + b)->size;
            }
        }
        //fprintf(stderr, "block %ld \n", sent/BLOCK_SIZE);
    }
    free(buf);
    pthread_exit((void *) 0);
}

void * fnv_hash_worker(void * a) {
    fnv_thread_arg * arg = (fnv_thread_arg *) a;
    for (long i = arg->range[0]; i < arg->range[1]; i++) {
        fnv_hash_block(arg->p, i, arg->buf);
    }
    pthread_exit((void *) 0);
}

void * fnv_hash_all(s_job * p, long blk_count) {
    pthread_attr_t attr;
    fnv_thread_arg arg[FNV_HASH_TNUM];
    pthread_t worker[FNV_HASH_TNUM];
    void *status; // pthread status
    for (int i = 0; i < FNV_HASH_TNUM; i++) {
        arg[i].range[0] = blk_count / FNV_HASH_TNUM * i;
        arg[i].range[1] = blk_count / FNV_HASH_TNUM * (i + 1);
        arg[i].p = p;
    }
    arg[FNV_HASH_TNUM - 1].range[1] = blk_count;//last thread must make the work done
    //if size == 13 FNV_HASH_TNUM == 4;range will be 0~3 3~6 6~9 9~13
    //initialize the pthread attrib for every worker
    pthread_attr_init(&attr);
    //set attr state for join() in the mother
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, MYSTACKSIZE);
    for (int i = 0; i < FNV_HASH_TNUM; i++) {
        //fprintf(log_fp, "pthread_c %d %d\n",arg[i].range[0],arg[i].range[1] );
        //fflush(log_fp);
        pthread_create(&worker[i], &attr, fnv_hash_worker_f, &(arg[i]));
    }
    // pthread_attr_* moved to main();

    for (int i = 0; i < FNV_HASH_TNUM; i++) {
        pthread_join(worker[i], &status);
    }

    //do some clean
    pthread_attr_destroy(&attr);
    return (void *) p;
}
