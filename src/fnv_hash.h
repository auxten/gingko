/*
 * fnv_hash.h
 *
 *  Created on: 2011-5-9
 *      Author: auxten
 */
#include "gingko.h"

#ifndef FNV_HASH_H_
#define FNV_HASH_H_
// arguments for thread
typedef struct {
    long range[2];
    s_job * p;
    unsigned char * buf;
} fnv_thread_arg;

inline unsigned fnv_hash(void *key, int len, unsigned h) {
    unsigned char *p = (unsigned char *) key;
    h = h ? h : 2166136261;

    for (int i = 0; i < len; i++)
        h = (h * 16777619) ^ p[i];

    return h;
}

void * fnv_hash_all(s_job * p, long blk_count);
#endif /* FNV_HASH_H_ */
