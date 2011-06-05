/*
 * path.cc
 *
 *  Created on: 2011-5-11
 *      Author: auxten
 */
#include <string.h>
#include "gingko.h"

int inplace_strip_tailing_slash(char * path) {
    if (path == NULL) {
        perr("passed NULL p\n");
        return -1;
    }
    char * p = path;
    for (int i = strlen(path) - 1; i > 0; i--) {
        if (*(p + i) == '/') {
            *(p + i) = '\0';
        } else {
            break;
        }
    }
    return 0;
}

int base_name(char * out, const char * in) {
    if (in == NULL) {
        perr("passed NULL p\n");
        return -1;
    }
    char * p = (char *) in;
    int i;
    int len = strlen(in);
    for (i = len; i > 0; i--) {
        if (*(p + i) == '/') {
            i++;
            break;
        }
    }
    if (out)
        strcpy(out, p + i);
    /*
     * return the dir path len
     * for example:
     * /home/work/opdir -> 	11
     * ./dir			-> 	2
     * dir				-> 	0
     * ../file			-> 	3
     *
     */
    return i;
}

int merge_path(char * out, const char * dir_name, const char * base_name) {
    if (out == NULL || dir_name == NULL || base_name == NULL) {
        perr("passed NULL p\n");
        return -1;
    }
    strcpy(out, dir_name);
    inplace_strip_tailing_slash(out);
    strcat(out, "/");
    strcat(out, base_name);
    if (strlen(out) == MAX_PATH_LEN) {
        perr("path too long\n");
        return -1;
    }
    return 0;
}

int inplace_change_path(char * path, const char * arg_1, const char * arg_2) {
    char tmp[MAX_PATH_LEN] = "\0";
    char tmp2[MAX_PATH_LEN] = "\0";
    strncpy(tmp2, arg_1, MAX_PATH_LEN);
    //printf("tmp2: %s\n", tmp2);
    inplace_strip_tailing_slash(tmp2);
    //printf("tmp2: %s\n", tmp2);
    int d = base_name(NULL, tmp2);
    //printf("d: %d,path+d: %s\n", d, tmp2+d);
    strncpy(tmp, path + d, MAX_PATH_LEN);
    merge_path(path, arg_2, tmp);
    return 0;
}
