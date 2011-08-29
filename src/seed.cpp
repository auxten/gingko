/**
 *  recurse_dir.cpp
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 **/
#ifndef GINGKO_SERV
#define GINGKO_SERV
#endif /** GINGKO_SERV **/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "gingko.h"
#include "hash/xor_hash.h"
#include "log.h"
#include "path.h"
#include "job_state.h"

/**
 * @brief for use of damn ftw(), we need these TLS(Thread Local Stack)
 * @brief global vars for every thread
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
extern pthread_key_t g_dir_key;

/**
 * @brief init the s_file_t array
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int init_struct(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info)
{
    long i = 0;
    s_seed_t *p_dir = (s_seed_t *) pthread_getspecific(g_dir_key);
    if (!access(name, R_OK))
    {
        if (type == FTW_D)
        {
            (p_dir->files + p_dir->init_s_file_t_iter)->size = -1;
        }
        else if (type == FTW_SL)
        {
            (p_dir->files + p_dir->init_s_file_t_iter)->size = -2;
            if (-1 == readlink(name,
                    (p_dir->files + p_dir->init_s_file_t_iter)->sympath,
                    MAX_PATH_LEN))
            {
                gko_log(FATAL, "readlink() error");
            }
        }
        else if (type == FTW_F && path_type(name) == GKO_FILE)
        {
            (p_dir->files + p_dir->init_s_file_t_iter)->size = status->st_size;
            ///printf("s: %d, e: %d", BLOCK_SIZE, BLOCK_COUNT(tmp_size+status->st_size));
            for (i = p_dir->tmp_size / (long) (BLOCK_SIZE); i
                    < BLOCK_COUNT(p_dir->tmp_size+status->st_size); i++)
            {
                if (i - p_dir->last_init_block == 1)
                {
                    (p_dir->blocks + i)->size = BLOCK_SIZE;
                    (p_dir->blocks + i)->start_f = p_dir->init_s_file_t_iter;
                    (p_dir->blocks + i)->start_off = i * BLOCK_SIZE
                            - p_dir->tmp_size;
                    p_dir->last_init_block = i;
                }
            }
            p_dir->tmp_size += status->st_size;
        }
        else
        {
            gko_log(FATAL, "not supported file type for %s", name);
            return 0;
        }

        memcpy(&((p_dir->files + p_dir->init_s_file_t_iter)->f_stat), status,
                sizeof(struct stat));
        (p_dir->files + p_dir->init_s_file_t_iter)->mode = status->st_mode;
        memcpy((p_dir->files + p_dir->init_s_file_t_iter)->name, name,
                strlen(name) + 1);
        p_dir->init_s_file_t_iter++;
    }
    else
    {
        gko_log(FATAL, "have no read permission for %s", name);
    }
    pthread_setspecific(g_dir_key, p_dir);
    return 0;
}

/**
 * @brief count the file num
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int file_counter(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info)
{
    s_seed_t *p_dir = (s_seed_t *) pthread_getspecific(g_dir_key);
    if (((type == FTW_F && path_type(name) == GKO_FILE) || type == FTW_SL
            || type == FTW_D))
    {
        if (!access(name, R_OK))
        {
            p_dir->file_count++;
            if (type == FTW_F)
            {
                p_dir->total_size += status->st_size;
            }
        }
        else
        {
            gko_log(FATAL, "have no read permission for %s", name);
        }
        ///printf("%ld", total_size);
    }
    pthread_setspecific(g_dir_key, p_dir);
    return 0;
}

/**
 * @brief count the total file size, only file !!!
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int init_total_count_size(s_job_t * jo)
{
    s_seed_t *p_dir = (s_seed_t *) pthread_getspecific(g_dir_key);
    int type = path_type(jo->path);
    if ((type & GKO_ERR) || (type & GKO_LINK))
    {
        p_dir->file_count = 0;
        p_dir->total_size = 0;
        jo->job_state = JOB_FILE_TYPE_ERR;
        gko_log(FATAL, "source %s is symlink", jo->path);
    }
    else
    {
        if (type & GKO_FILE)
        {
            /** if it's a file or symlink to file **/
            struct stat file_stat;
            if(stat(jo->path, &file_stat) != 0)
            {
                gko_log(FATAL, "stat file %s error", jo->path);
                return -1;
            }
            file_counter(jo->path, &file_stat, FTW_F, NULL);
        }
        else if (type & GKO_DIR)
        {
            /** if it's a dir **/
            if(nftw(jo->path, file_counter, MAX_SCAN_DIR_FD, FTW_PHYS) != 0)
            {
                gko_log(FATAL, "nftw path %s error", jo->path);
                return -1;
            }
        }
        else
        {
            jo->job_state = JOB_FILE_TYPE_ERR;
            gko_log(FATAL, "the reqeusted path %s type is special", jo->path);
            return -1;
        }
    }
    /**
     *  init the files_size blocks_size
     **/
    GKO_INT64 blk_cnt = BLOCK_COUNT(p_dir->total_size);
    jo->total_size = p_dir->total_size;
    jo->files_size = p_dir->file_count * sizeof(s_file_t);
    jo->blocks_size = blk_cnt * sizeof(s_block_t);
    jo->block_count = blk_cnt;
    jo->file_count = p_dir->file_count;

    return 0;
}

/**
 * @brief the init_struct caller
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
GKO_STATIC_FUNC int init_seed(s_job_t * jo)
{
    s_seed_t *p_dir = (s_seed_t *) pthread_getspecific(g_dir_key);
    int type = path_type(jo->path);
    if ((type & GKO_ERR) || (type & GKO_LINK))
    {
        p_dir->file_count = 0;
        p_dir->total_size = 0;
        jo->job_state = JOB_FILE_TYPE_ERR;
        gko_log(FATAL, "source %s is symlink", jo->path);
    }
    else
    {
        if (type & GKO_FILE)
        {
            /** if it's a file or symlink to file **/
            struct stat file_stat;
            if(stat(jo->path, &file_stat) != 0)
            {
                gko_log(FATAL, "stat file %s error", jo->path);
                return -1;
            }
            init_struct(jo->path, &file_stat, FTW_F, NULL);
        }
        else if (type & GKO_DIR)
        {
            if(nftw(jo->path, init_struct, MAX_SCAN_DIR_FD, FTW_PHYS) != 0)
            {
                gko_log(FATAL, "nftw path %s error", jo->path);
                return -1;
            }
        }
        else
        {
            jo->job_state = JOB_FILE_TYPE_ERR;
            gko_log(FATAL, "the reqeusted path %s type is special", jo->path);
            return -1;
        }
    }
    for (int i = 0; i < BLOCK_COUNT(p_dir->total_size); i++)
    {
        ///init the block.done
        (p_dir->blocks + i)->done = 1;
    }

    return 0;
}

/** temporarily not used
static int list_file(const char *name, const struct stat *status, int type,
        struct FTW * ftw_info)
{
    if (type == FTW_NS)
        return 0;

    if (type == FTW_F)
        gko_log(NOTICE, "0%3o\tFile\t\t%lld\t\t\t%s", status->st_mode & 0777,
                status->st_size, name);

    if (type == FTW_D && strcmp(".", name) != 0)
        gko_log(NOTICE, "0%3o\tDir\t\t%lld\t\t\t%s", status->st_mode & 0777,
                status->st_size, name);

    if (type == FTW_SL)
        gko_log(NOTICE, "0%3o\tLink\t\t%lld\t\t\t%s", status->st_mode & 0777,
                status->st_size, name);

    return 0;
}
**/


/**
 * @brief init the s_job_t s_block_t s_file_t struct for the job
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int recurse_dir(s_job_t * jo)
{
    s_seed_t dir;
    s_seed_t * p_dir = &dir;
    p_dir->file_count = 0;
    p_dir->init_s_file_t_iter = 0;
    p_dir->init_s_block_t_iter = 0;
    p_dir->total_size = 0;
    p_dir->tmp_size = 0;
    p_dir->last_init_block = -1;
    pthread_setspecific(g_dir_key, p_dir);
    p_dir = (s_seed_t *) pthread_getspecific(g_dir_key);

    /**
     *  as usage of global static, get_file_count can only run once in one proc
     **/
    GKO_INT64 blk_cnt;
    GKO_INT64 min_full_blk_cnt;
    if(init_total_count_size(jo) != 0)
    {
        gko_log(FATAL, "init_total_count_size error");
        return -1;
    }
    blk_cnt = BLOCK_COUNT(p_dir->total_size);
    min_full_blk_cnt = p_dir->total_size ? (blk_cnt - 1) : 0;

    if (p_dir->file_count <= 1)
    {
        ///the only file is the dir
        gko_log(NOTICE, "path: %s, p_dir->file_count is %lld", jo->path,
                p_dir->file_count);
    }
    if (p_dir->file_count)
    {
        p_dir->files = (s_file_t *) calloc(p_dir->file_count, sizeof(s_file_t));
    }
    if (! p_dir->files)
    {
    	gko_log(FATAL, "calloc for files failed");
    	return -1;
    }
    if (p_dir->total_size)
    {
        p_dir->blocks = (s_block_t *) calloc(blk_cnt, sizeof(s_block_t));
    }
    if (! p_dir->blocks)
    {
    	gko_log(FATAL, "calloc for blocks failed");
    	return -1;
    }
    gko_log(NOTICE, "s_file_t size: %lu", sizeof(s_file_t));
    gko_log(NOTICE, "file count: %lld", p_dir->file_count);
    gko_log(NOTICE, "total size: %lld", p_dir->total_size);
    gko_log(NOTICE, "block count: %lld", blk_cnt);

    /**
     *  init the files and blocks, and init the last block
     **/
    if(init_seed(jo) != 0)
    {
        gko_log(FATAL, "init seed error");
        return -1;
    }

    jo->files = p_dir->files;
    if (p_dir->total_size)
    {
        (p_dir->blocks + min_full_blk_cnt)->size = p_dir->total_size
                - min_full_blk_cnt * BLOCK_SIZE;

        GKO_INT64 i = p_dir->total_size;
        GKO_INT64 j = p_dir->file_count - 1;
        GKO_INT64 file_size_tmp = (p_dir->files + j)->size;
        for (; i - MAX(file_size_tmp, 0) > min_full_blk_cnt * BLOCK_SIZE && j
                >= 0; i -= MAX(file_size_tmp, 0), j--)
        {
            file_size_tmp = (p_dir->files + j)->size;
            ///printf("start_f: %d, off: %d", j, i);
        }

        /**
         *  check the last block
         **/
        if (((p_dir->blocks + min_full_blk_cnt)->start_f != j)
                || ((p_dir->blocks + min_full_blk_cnt)->start_off
                        != (min_full_blk_cnt) * BLOCK_SIZE - (i - (p_dir->files
                                + j)->size)))
        {
            gko_log(NOTICE, "file division maybe error!");
        }
        jo->blocks = p_dir->blocks;

    }

    for (GKO_INT64 i = 0; i < p_dir->file_count; i++)
    {
        gko_log(NOTICE, "0%3o\t\t%lld\t\t\t%s\t%s",
                (p_dir->files + i)->mode & 0777, (p_dir->files + i)->size,
                (p_dir->files + i)->name, (p_dir->files + i)->sympath);
    }

    return 0;
}
