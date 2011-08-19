/**
 * xor_hash.h
 *
 *  Created on: 2011-5-9
 *      Author: auxten
 **/
#include "../gingko.h"

#ifndef XOR_HASH_H_
#define XOR_HASH_H_

/// shift Macro
#define ROLL(h) (((h) << 7) ^ ((h) >> 25))
/// arguments for thread
typedef struct
{
    GKO_INT64 range[2];
    s_job_t * p;
    u_char * buf;
    int index;
} hash_worker_thread_arg;

/// xor hash a given length buf
unsigned xor_hash(const void *key, int len, unsigned hval);
/// check if the fnv check sum is OK
char digest_ok(void * buf, s_block_t * b);
/// xor hash all blocks for a job given
int xor_hash_all(s_job_t * jo, hash_worker_thread_arg arg[]);
#endif /** XOR_HASH_H_ **/
