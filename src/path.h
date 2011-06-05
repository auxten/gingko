/*
 * path.h
 *
 *  Created on: 2011-5-11
 *      Author: auxten
 */

#ifndef PATH_H_
#define PATH_H_

int inplace_strip_tailing_slash(char * path);
int base_name(char * in, char * out);
int merge_path(char * out, const char * dir_name, const char * base_name);
int inplace_change_path(char * path, const char * arg_1, const char * arg_2);
#endif /* PATH_H_ */
