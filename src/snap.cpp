/**
 * snap.cpp
 *
 *  Created on: 2011-7-14
 *      Author: auxten
 **/
#ifndef GINGKO_CLNT
#define GINGKO_CLNT
#endif /** GINGKO_CLNT **/

#include "gingko.h"
#include "log.h"

extern s_gingko_global_t gko;

/**
 * @brief dump the progress of this blk
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
void dump_progress(s_job_t * jo, s_block_t * blk)
{
    /**
     * frist of all fsync the wrote block
     * use pwrite to dump every blk write progress
     **/
    s_snap_t snap = {'\0'};
    snap.digest = blk->digest;
    snap.done = 1;

    if (gko.snap_fd < 0)
    {
        if ((gko.snap_fd = open(gko.snap_fpath, SNAP_OPEN_FLAG, SNAP_FILE_MODE))
                < 0)
        {
            gko_log(WARNING, "open snap file failed");
            return;
        }
    }

    /**
     * calculate the offset of block in snap file
     **/
    off_t offset = (blk - jo->blocks) * sizeof(s_snap_t);
    if (FAIL_CHECK(pwrite(gko.snap_fd, &snap, sizeof(s_snap_t), offset) == -1))
    {
        gko_log(WARNING, "pwrite snap failed");
    }
    fsync(gko.snap_fd);
    return;
}

/**
 * @brief examine the snap file handled by fd, if OK load it
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int load_snap(s_job_t * jo)
{
    struct stat snap_fstat;

    if (gko.snap_fd < 0)
    {
        if ((gko.snap_fd = open(gko.snap_fpath, SNAP_OPEN_FLAG, SNAP_FILE_MODE))
                < 0)
        {
            gko_log(WARNING, "open snap file failed");
            return -1;
        }
    }

    /**
     * first check the snap file size and the blk count * s_snap_t
     **/
    if (fstat(gko.snap_fd, &snap_fstat) < 0)
    {
        gko_log(WARNING, "fstat snap file failed");
        return -1;
    }
    if ((GKO_UINT64) snap_fstat.st_size != jo->block_count * sizeof(s_snap_t))
    {
        gko_log(WARNING, "snap file size mismatch");
        return -1;
    }
    /**
     * check the existed file size
     **/
    /// this is done in mk_dir_symlink_file()

    /**
     * read the snap file
     **/
    char * snap_buf;
    if (readfileall(gko.snap_fd, 0, snap_fstat.st_size, &snap_buf))
    {
        gko_log(WARNING, "snap file read error");
        return -1;
    }
    /**
     * compare digest and set block done
     **/
    s_snap_t * snap_p = (s_snap_t *) snap_buf;
    s_block_t * blk_p = jo->blocks;
    for (int i = 0; i < jo->block_count; i++)
    {
        if ((blk_p + i)->digest == (snap_p + i)->digest && (snap_p + i)->done)
        {
            gko_log(NOTICE, "block %d downloaded previously", i);
            (blk_p + i)->done = 1;
        }
    }
    free(snap_buf);
    snap_buf = NULL;
    return 0;
}
