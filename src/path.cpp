/**
 * path.cpp
 *
 *  Created on: 2011-5-11
 *      Author: auxten
 **/
#include <string.h>
#include <sys/stat.h>
#include "gingko.h"
#include "hash/xor_hash.h"
#include "log.h"
#include "path.h"

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
int inplace_strip_tailing_slash(char * path)
{
    if (path == NULL)
    {
        gko_log(WARNING, "passed NULL p");
        return -1;
    }
    char * p = path;
    for (int i = strlen(path) - 1; i > 0; i--)
    {
        if (*(p + i) == '/')
        {
            *(p + i) = '\0';
        }
        else
        {
            break;
        }
    }
    return 0;
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
int inplace_add_tailing_slash(char * path)
{
    if (path == NULL)
    {
        gko_log(WARNING, "passed NULL p");
        return -1;
    }
    char * p = path;
    for (int i = strlen(path) - 1; i > 0; i--)
    {
        if (*(p + i) == '/')
        {
            *(p + i) = '\0';
        }
        else
        {
            break;
        }
    }
    strncat(path, "/", MAX_PATH_LEN - strlen(path));
    return 0;
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
int get_base_name_index(char * out, const char * in)
{
    if (in == NULL)
    {
        gko_log(WARNING, FLF("passed NULL p at get_base_name_index"));
        return -1;
    }
    char * p = (char *) in;
    int i;
    int len = strlen(in);
    if (!len)
    {
        gko_log(WARNING, FLF("input string len == 0"));
        return -1;
    }
    for (i = len; i > 0; i--)
    {
        if (*(p + i) == '/')
        {
            i++;
            break;
        }
    }
    if (out)
    {
        strncpy(out, p + i, MAX_PATH_LEN);
    }
    return i;
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
int merge_path(char * out, const char * dir_name, const char * base_name)
{
    if (!out || !dir_name || !base_name)
    {
        gko_log(WARNING, "passed NULL p");
        return -1;
    }
    strncpy(out, dir_name, MAX_PATH_LEN);
    inplace_strip_tailing_slash(out);
    strncat(out, "/", MAX_PATH_LEN - strlen(out));
    strncat(out, base_name, MAX_PATH_LEN - strlen(out));
    if (strlen(out) == MAX_PATH_LEN)
    {
        gko_log(WARNING, "path too long");
        return -1;
    }
    return 0;
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
int change_to_local_path(char * path, const char * req_path,
        const char * local_path, char dst_path_exist)
{
    if (!path || !req_path || !local_path)
    {
        gko_log(FATAL, "%s %s passed NULL p", __FILE__, __func__);
        return -1;
    }
    char base_name[MAX_PATH_LEN] = "\0";
    char tmp_req_path[MAX_PATH_LEN] = "\0";
    strncpy(tmp_req_path, req_path, MAX_PATH_LEN); /// ../test
    ///printf("tmp2: %s", tmp2);
    inplace_strip_tailing_slash(tmp_req_path); /// ../test
    ///printf("tmp2: %s", tmp2);
    int d;
    if (dst_path_exist)
    {
        d = get_base_name_index(NULL, tmp_req_path);
    }
    else
    {
        d = strlen(tmp_req_path) + 1;
    }

    if (d < 0)
    {
        gko_log(FATAL, FLF("change_to_local_path failed"));
        return -1;
    }
    ///printf("d: %d,path+d: %s", d, tmp2+d);
    strncpy(base_name, path + d, MAX_PATH_LEN); /// test/.DS_Store
    merge_path(path, local_path, base_name); /// ../output2/ + test/.DS_Store
    inplace_strip_tailing_slash(path); /// ../test
    return 0;
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
GKO_STATIC_FUNC char * cwd_path_to_abs_path(char * abs_path, const char * oldpath)
{
    if (!abs_path || !oldpath)
    {
        gko_log(FATAL, "%s %s passed NULL p", __FILE__, __func__);
        return NULL;
    }
    if ((strlen(oldpath) < 1))
    {
        gko_log(WARNING, "invalid oldpath");
        return NULL;
    }
    else
    {
        if (*oldpath == '/') ///already a abs path
        {
            strncpy(abs_path, oldpath, MAX_PATH_LEN);
        }
        else
        {
            if (!(getcwd(abs_path, MAX_PATH_LEN)))
            {
                gko_log(WARNING, "getcwd error");
                return NULL;
            }
            inplace_add_tailing_slash(abs_path);
            strncat(abs_path, oldpath, MAX_PATH_LEN - strlen(abs_path));
        }
        return abs_path;
    }
}

/**
 * @brief get the symlink dest's absolute path, store it in abs_path
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
char * symlink_dest_to_abs_path(char * abs_path, const char * symlink)
{
    if (!abs_path || !symlink)
    {
        gko_log(FATAL, "%s %s passed NULL p", __FILE__, __func__);
        return NULL;
    }

    memset(abs_path, 0, MAX_PATH_LEN);
    /** read the symlink first **/
    if (readlink(symlink, abs_path, MAX_PATH_LEN) < 0)
    {
        gko_log(WARNING, "read synlink '%s' dest failed", symlink);
        return NULL;
    }

    /** if starts with a '/', that's it!! **/
    if (*abs_path == '/')
    {/** symlink to a abs path **/
        gko_log(NOTICE, "symlink to a abs path:)");
        return abs_path;
    }
    else
    {/** symlink to a relative path, need convert **/
        char tmp_sympath[MAX_PATH_LEN];
        strncpy(tmp_sympath, abs_path, MAX_PATH_LEN);
        if (!cwd_path_to_abs_path(abs_path, symlink))
        {
            gko_log(WARNING, "cwd_path_to_abs_path failed '%s' '%s'", abs_path,
                    symlink);
            return NULL;
        }
        int idx = get_base_name_index(NULL, abs_path);
        if (idx < 0)
        {
            gko_log(FATAL, "get_base_name_index failed: '%s'", abs_path);
            return NULL;
        }
        *(abs_path + idx) = '\0';
        strncat(abs_path, tmp_sympath, MAX_PATH_LEN);
        return abs_path;
    }

}

/**
 * @brief generate snap file path with requested localpath and requested remote uri
 *
 * @see
 * @note
 *     return the hash result from xor_hash(uri, strlen(uri), 0)
 *     the snap file path is stored in snap_fpath
 * @return  0 for error
 *          uri_hash for succ
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
unsigned gen_snap_fpath(char *snap_fpath, const char * localpath,
        const char * uri)
{
    if (!snap_fpath || !localpath || !uri)
    {
        gko_log(FATAL, "%s %s passed NULL p", __FILE__, __func__);
        return 0;
    }

    char out_path[MAX_PATH_LEN] = {'\0'};
    strncpy(out_path, localpath, MAX_PATH_LEN);
    inplace_strip_tailing_slash(out_path);

    if (!cwd_path_to_abs_path(snap_fpath, out_path))
    {
        gko_log(WARNING, "cwd_path_to_abs_path failed '%s' '%s'", snap_fpath,
                out_path);
        return 0;
    }
    /** cut the tailing file in path **/
    if (!(path_type(snap_fpath) & GKO_DIR))
    {
        int idx;
        if ((idx = get_base_name_index(NULL, snap_fpath)) < 0)
        {
            gko_log(WARNING, "gen_snap_fpath failed '%s' '%s' '%s'", snap_fpath,
                    out_path, uri);
            return 0;
        }
        *(snap_fpath + idx) = '\0';
    }

    int snap_fpath_len = strlen(snap_fpath);
    unsigned uri_hash = xor_hash(uri, strlen(uri), 0);
    snprintf(snap_fpath + snap_fpath_len, MAX_PATH_LEN - snap_fpath_len,
            "/%s%u", GKO_SNAP_FILE, uri_hash);

    gko_log(NOTICE, "gko.snap_fpath: '%s'", snap_fpath);
    return uri_hash;
}

/**
 * @brief get the dest type, return:
 *
 * @see
 * @note
 *     GKO_FILE =    0001;    ///file and for file test.eg: flag & GKO_FILE
 *     GKO_DIR  =    0002;    ///dir and for dir test
 *     GKO_NONE =    0004;    ///nonexisted and for nonexisted test
 *     GKO_OTHR =    0000;    ///other
 *     GKO_LFILE =   0011;    ///symlink to file
 *     GKO_LDIR =    0012;    ///symlink to dir
 *     GKO_LNONE =   0014;    ///symlink to nonexist
 *     GKO_LOTHR =   0010;    ///symlink to other
 *     GKO_LINK =    0010;    ///for symlink test
 *     GKO_ERR =     0100;    ///error
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int path_type(const char * p)
{
    if (!p)
    {
        gko_log(FATAL, FLF("null passed to path_type"));
        return GKO_ERR;
    }
    struct stat dest_stat;
    char path[MAX_PATH_LEN];
    strncpy(path, p, MAX_PATH_LEN);
    inplace_strip_tailing_slash(path);
    if (lstat(path, &dest_stat) < 0)
    {
        if (errno == ENOENT)
        {
            gko_log(NOTICE, "non existed path '%s'", path);
            return GKO_NONE;
        }
        gko_log(WARNING, "stat dest_stat failed %d", errno);
        return GKO_ERR;
    }
    else
    {
        switch (dest_stat.st_mode & S_IFMT)
        {
            case S_IFREG:
                return GKO_FILE;

            case S_IFDIR:
                return GKO_DIR;

            case S_IFLNK:
                {
                struct stat symstat;
                if (stat(path, &symstat) < 0)
                {
                    if (errno == ENOENT)
                    {
                        gko_log(WARNING, "non existed sympath '%s'", path);
                        return GKO_LNONE;
                    }
                    gko_log(WARNING, "stat symstat failed %d", errno);
                    return GKO_ERR;
                }
                else
                {
                    switch (symstat.st_mode & S_IFMT)
                    {
                        case S_IFREG:
                            return GKO_LFILE;

                        case S_IFDIR:
                            return GKO_LDIR;

                        default:
                            gko_log(WARNING,
                                    "symbol path '%s' not a regular file or dir",
                                    path);
                            return GKO_LOTHR;
                    }
                }
                break;
            }

            default:
                gko_log(WARNING, "'%s' not a regular file or dir", path);
                return GKO_OTHR;
        }
    }
}

/**
 * @brief make dir and symlink, create the file and check the size
 *
 * @see
 * @note
 *     if size not matched, continue flag is canceled. all file
 *     will be truncate to the expected size.
 *     if the symlink is already exist, unlink it and create a
 *     new one
 *     if the dir is already exist, leave it there
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int mk_dir_symlink_file(s_job_t * jo, char * to_continue)
{
    if (!jo || !to_continue)
    {
        gko_log(FATAL, FLF("null pointer"));
        return -1;
    }
    s_file_t * tmp;
    for (GKO_INT64 i = 0; i < jo->file_count; i++)
    {
        tmp = jo->files + i;
        int fd = -1;
        gko_log(NOTICE, "0%3o\t%lld\t%s\t%s", tmp->mode & 0777, tmp->size,
                (jo->files + i)->name, tmp->sympath);
        switch (tmp->size)
        {
            /**
             * to make sure client can write to every file transfered,
             * we create the dir and file with mode |= S_IWUSR
             * we will correct file, dir mode after transfer is over.
             **/
            case -1: /// dir
                if (mkdir(tmp->name, tmp->mode|S_IWUSR) && errno != EEXIST)
                {
                    gko_log(FATAL, "mkdir error");
                    return -1;
                }
                break;

            case -2: /// symbol link
                if (symlink(tmp->sympath, tmp->name))
                {
                    if (errno == EEXIST)
                    {
                        if (unlink(tmp->name))
                        {
                            gko_log(FATAL, "unlink existed symlink '%s' error",
                                    tmp->name);
                        }
                        if (symlink(tmp->sympath, tmp->name))
                        {
                            gko_log(FATAL, "re-create symlink '%s' to '%s' error",
                                    tmp->name, tmp->sympath);
                        }
                    }
                    else
                    {
                        gko_log(FATAL, "symlink error");
                        return -1;
                    }
                }
                break;

            default: ///regular file
                if (-1 == (fd = open(tmp->name, CREATE_OPEN_FLAG, tmp->mode|S_IWUSR)))
                {
                    gko_log(FATAL, "make or open new file error");
                    return -1;
                }
                else
                {
                    struct stat f_stat;
                    /// if to_continue flag is on, but the file size existed
                    /// doesn't match the one in seed. cancel the to_continue
                    /// flag.
                    if (*to_continue)
                    {
                        if (fstat(fd, &f_stat))
                        {
                            gko_log(
                                    WARNING,
                                    "fstat file '%s' error, continue flag canceled",
                                    tmp->name);
                            *to_continue = 0;
                        }
                        if (f_stat.st_size != tmp->size)
                        {
                            gko_log(
                                    WARNING,
                                    "file '%s' size not matched, continue flag canceled",
                                    tmp->name);
                            *to_continue = 0;
                        }
                    }
                    if (ftruncate(fd, tmp->size))
                    {
                        gko_log(FATAL, "truncate file error");
                        close(fd);
                        return -1;
                    }
                    close(fd);
                }
                break;
        }
    }
    return 0;
}

/**
 * @brief correct the file and dir mode, cause for write in we create them with mode|S_IWUSR
 *          and take care of the setgit of dir
 *
 * @see http://www.gnu.org/software/coreutils/manual/html_node/Directory-Setuid-and-Setgid.html
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-10
 **/
int correct_mode(s_job_t * jo)
{
    s_file_t * f_p;
    for (GKO_INT64 i = 0; i < jo->file_count; i++)
    {
        f_p = jo->files + i;
        if ((f_p->size == -1) || (!(f_p->mode & S_IWUSR)))
        {
            if (chmod(f_p->name, f_p->mode))
            {
                gko_log(FATAL, "chmod for '%s' to 0%3o failed!!!", f_p->name,
                        f_p->mode & 0777);
                return -1;
            }
            gko_log(TRACE, "chmod for '%s' to 0%3o succeed", f_p->name,
                    f_p->mode & 0777);
        }
    }

    return 0;
}
/**
 * @brief convert the remote path recv from JOIN to local path
 *
 * @see
 * @note
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int process_path(s_job_t * jo)
{
    char out_path[MAX_PATH_LEN] = {'\0'};
    strncpy(out_path, jo->path, MAX_PATH_LEN);
    inplace_strip_tailing_slash(out_path);
    int out_type = path_type(out_path);
    if (out_type & GKO_NONE)
    {/** non-existed dest or dest is symlink to non-existed **/
        if (out_type & GKO_LINK)
        {/** dest is symlink to non-existed **/
            gko_log(FATAL, "destination: '%s' is a symlink to non-exist",
                    out_path);
            return -1;
        }
        else if ((jo->file_count == 1 && jo->files->size >= 0) &&
                (jo->path)[strlen(jo->path) - 1] == '/')
        {/** dest is a non existed dir path like './non/' but job is a singal file **/
            gko_log(FATAL, "downloading a file to non existed path '%s'",
                    jo->path);
            return -1;
        }
        else
        { /** non-existed dest **/
            /** determine if the base dir is existed **/
            char out_base_path[MAX_PATH_LEN];
            //strncpy(out_base_path, out_path, MAX_PATH_LEN);
            cwd_path_to_abs_path(out_base_path, out_path);

            int base_idx = get_base_name_index(NULL, out_base_path);
            if (base_idx < 0)
            {
                gko_log(FATAL, "process_path failed");
                return -1;
            }
            out_base_path[base_idx] = '\0';
            int out_base_type = path_type(out_base_path);
            if (out_base_type & GKO_DIR)
            {/** out base path is a dir or symlink to dir **/
                for (int i = 0; i < jo->file_count; i++)
                {
                    if (FAIL_CHECK(
                            change_to_local_path((jo->files + i)->name, jo->uri, out_path, 0)))
                    {
                        gko_log(
                                FATAL,
                                "change to local path error, name: '%s', uri: '%s', path: '%s'",
                                (jo->files + i)->name, jo->uri, out_path);
                        return -1;
                    }
                    //gko_log(NOTICE, "path: '%s'", (jo->files + i)->name);
                }
                return 0;
            }
            else
            {/** out base path is not dir, nor symlink to dir **/
                gko_log(FATAL, "base path: '%s' is non-dir", out_base_path);
                return -1;
            }
        }
    }
    else
    {/** dest path existed **/
        if (out_type & GKO_FILE)
        {/** dest path is file or symlink to file **/
            if (jo->file_count == 1)
            {
                if ((jo->files)->size == -1)
                {/** dest path is dir **/
                    gko_log(FATAL, "can't overwrite dir: '%s' on file: '%s'",
                            (jo->files)->name, out_path);
                    return -1;
                }
                else if ((jo->files)->size == -2)
                {/** dest path is symlink **/
                    gko_log(FATAL, "can't overwrite symlink: '%s' on file: '%s'",
                            (jo->files)->name, out_path);
                    return -1;
                }
                else
                {/** dest path is file **/
                    strncpy((jo->files)->name, out_path, MAX_PATH_LEN);
                    return 0;
                }
            }
            else
            { /** file count != 1,
                indicating that job is a dir, dest is a existed file **/
                gko_log(FATAL, "can't overwrite dir: '%s' on file: '%s'",
                        (jo->files)->name, out_path);
                return -1;
            }
        }
        else if (out_type & GKO_DIR)
        {/** dest is a existed dir **/
            for (int i = 0; i < jo->file_count; i++)
            {
                if (FAIL_CHECK(change_to_local_path((jo->files + i)->name, jo->uri, out_path, 1)))
                {
                    gko_log(
                            FATAL,
                            "change to local path error, name: '%s', uri: '%s', path: '%s'",
                            (jo->files + i)->name, jo->uri, out_path);
                    return -1;
                }
            }
            return 0;
        }
        else
        {/** dest is non-regular file or dir **/
            gko_log(FATAL, "the dest: '%s' is non-regular file or dir", out_path);
            return -1;
        }
    }
    return 0;
}

