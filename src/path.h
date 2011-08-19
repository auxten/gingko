/**
 * path.h
 *
 *  Created on: 2011-5-11
 *      Author: auxten
 **/

#ifndef PATH_H_
#define PATH_H_

///file and for file test.eg: flag & GKO_FILE
static const int GKO_FILE =    0001;
///dir and for dir test
static const int GKO_DIR  =    0002;
///nonexisted and for nonexisted test
static const int GKO_NONE =    0004;
///other
static const int GKO_OTHR =    0000;
///symlink to file
static const int GKO_LFILE =   0011;
///symlink to dir
static const int GKO_LDIR =    0012;
///symlink to nonexist
static const int GKO_LNONE =   0014;
///symlink to other
static const int GKO_LOTHR =   0010;
///for symlink test
static const int GKO_LINK =    0010;
///error
static const int GKO_ERR =     0100;


/// ../path////   TO  ../path
int inplace_strip_tailing_slash(char * path);
/// ../path      TO  ../path/
int inplace_add_tailing_slash(char * path);
/// get the base name of a string
int get_base_name_index(char * in, const char * out);
/// merge into dir_name/base_name
int merge_path(char * out, const char * dir_name, const char * base_name);
/// change remote path to local path, store it in path
int change_to_local_path(char * path, const char * req_path,
        const char * local_path, char dst_path_exist);
/// get the symlink dest's absolute path, store it in abs_path
char * symlink_dest_to_abs_path(char * abs_path, const char * symlink);
/// generate snap file path with requested localpath and requested remote uri
unsigned gen_snap_fpath(char *snap_fpath, const char * localpath,
        const char * uri);
/// generate snap file path with requested localpath and requested remote uri
int path_type(const char * path);
/// generate snap file path with requested localpath and requested remote uri
int mk_dir_symlink_file(s_job_t * jo, char * to_continue);
/// correct the file and dir mode, cause for write in we create them with mode|S_IWUSR
int correct_mode(s_job_t * jo);
/// convert the remote path recv from JOIN to local path
int process_path(s_job_t * jo);

#endif /** PATH_H_ **/
