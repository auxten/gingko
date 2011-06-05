/*
 *  recurse_dir.cc
 *  gingko
 *
 *  Created by Auxten on 11-4-9.
 *  Copyright 2011 Baidu. All rights reserved.
 *
 */

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
#include "md5.h"
#include "fnv_hash.h"

/*
 *  for use of damn ftw(), we need these static...
 *  later we will use multi procesess for multi taskes
 */
extern pthread_key_t dir_key;

int empty_dir(char *path) {
	int num = 0;
	DIR *dir = opendir(path);
	while (readdir(dir) != NULL) {
		++num;
	}
	closedir(dir);
	return num == 2 ? 1 : 0;
}

int init_struct(const char *name, const struct stat *status, int type, struct FTW * ftw_info) {
	long i = 0;
	s_dir *p_dir = (s_dir *)pthread_getspecific(dir_key);
	if (strcmp(".", name) != 0) {
		memcpy(&((p_dir->files + p_dir->init_s_file_iter)->f_stat), status,
				sizeof(struct stat));
		(p_dir->files + p_dir->init_s_file_iter)->mode = status->st_mode;
		if (type == FTW_D) {
			(p_dir->files + p_dir->init_s_file_iter)->size = -1;
		} else if (type == FTW_SL) {
			(p_dir->files + p_dir->init_s_file_iter)->size = -2;
			if (-1 == readlink(name, (p_dir->files + p_dir->init_s_file_iter)->sympath,
					MAX_PATH_LEN)) {
				perror("readlink() error");
			}
		} else if (type == FTW_F) {
			(p_dir->files + p_dir->init_s_file_iter)->size = status->st_size;
			//printf("s: %d, e: %d\n", BLOCK_SIZE, BLOCK_COUNT(tmp_size+status->st_size));
			for (i = p_dir->tmp_size / (long) (BLOCK_SIZE); i
					< BLOCK_COUNT(p_dir->tmp_size+status->st_size); i++) {
				if (i - p_dir->last_init_block == 1) {
					(p_dir->blocks + i)->size = BLOCK_SIZE;
					(p_dir->blocks + i)->start_f = p_dir->init_s_file_iter;
					(p_dir->blocks + i)->start_off = i * BLOCK_SIZE - p_dir->tmp_size;
//					printf("i: %ld, start_f: %ld, start_off: %ld\n", i,
//							(blocks + i)->start_f, (blocks + i)->start_off);
					p_dir->last_init_block = i;
				}
			}
			p_dir->tmp_size += status->st_size;
		}
		memcpy((p_dir->files + p_dir->init_s_file_iter)->name, name, strlen(name) + 1);
		p_dir->init_s_file_iter++;
	}
	pthread_setspecific(dir_key, p_dir);
	return 0;
}

int init_block_md5(s_job * jo, long block_id) {
	s_file * files = jo->files;
	s_block * blocks = jo->blocks;
	long read_counter = 0;
	long file_i = 0;
	long offset = 0;
	unsigned char buf[BLOCK_SIZE];
	int fd;
	md5_context ctx;

	if (-1 == (fd = open((files + ((blocks + block_id)->start_f))->name,
			O_RDONLY | O_NOFOLLOW))) {
		perror("file open() error!");
	}
	memset(buf, 0, BLOCK_SIZE);
	offset = (blocks + block_id)->start_off;
	md5_starts(&ctx);
	while (read_counter < (blocks + block_id)->size) {
		long tmp;
		tmp = pread(fd, buf, (blocks + block_id)->size - read_counter, offset);
		switch (tmp) {
		case -1:
			perror("init_block_md5 pread error");
			break;

		case 0:
			close(fd);
			//if the next if a nonfile then next
			file_i = next_f(jo, file_i);
			if ( -1 == (fd = open((files + ((blocks + block_id)->start_f) + file_i)->name,
					O_RDONLY | O_NOFOLLOW))) {
                fprintf(stderr, "filename: %s\n", (files + ((blocks + block_id)->start_f) + file_i)->name);
                perror("file open() error!");
            }
			offset = 0;
			break;

		default:
			//printf("read: %ld\n", tmp);
			md5_update(&ctx, buf, (int) tmp);
			read_counter += tmp;
			offset += tmp;
			break;
		}
	}
	md5_finish(&ctx, (blocks + block_id)->md5);
	//    printf("buf: %d\n", sizeof(buf));
	//    memset(buf, 0, sizeof(buf));
	//    printf("buf: %d\n", sizeof(buf));
	close(fd);
	return 0;
}

int init_file_md5(s_job * jo, long file_id) {
	s_file * files = jo->files;
	if (md5_file((files + file_id)->name, (files + file_id)->md5)) {
		perror("md5_file() error!");
		return 1;
	}
	return 0;
}

int get_file_count(const char *path) {
	nftw(path, file_counter, 50, FTW_PHYS);
	return 0;
}

int file_counter(const char *name, const struct stat *status, int type,
		struct FTW * ftw_info) {
	s_dir *p_dir = (s_dir *)pthread_getspecific(dir_key);
	if ((type == FTW_F || type == FTW_SL || type == FTW_D) && strcmp(".", name)) {
		p_dir->file_count++;
		if (type == FTW_F) {
			p_dir->total_size += status->st_size;
		}
		//printf("%ld\n", total_size);
	}
	pthread_setspecific(dir_key, p_dir);
	return 0;
}

int list_file(const char *name, const struct stat *status, int type,
		struct FTW * ftw_info) {
	if (type == FTW_NS)
		return 0;

	if (type == FTW_F)
		printf("0%3o\tFile\t\t%lld\t\t\t%s\n", status->st_mode & 0777,
				status->st_size, name);

	if (type == FTW_D && strcmp(".", name) != 0)
		printf("0%3o\tDir\t\t%lld\t\t\t%s/\n", status->st_mode & 0777,
				status->st_size, name);

	if (type == FTW_SL)
		printf("0%3o\tLink\t\t%lld\t\t\t%s/\n", status->st_mode & 0777,
				status->st_size, name);

	return 0;
}

int recurse_dir(s_job * jo) {
	s_dir dir;
	s_dir * p_dir = &dir;
	p_dir->file_count = 0;
	p_dir->init_s_file_iter = 0;
	p_dir->init_s_block_iter = 0;
	p_dir->total_size = 0;
	p_dir->tmp_size = 0;
	p_dir->last_init_block = -1;
	pthread_setspecific(dir_key, p_dir);

	/*
	 *  as usage of global static, get_file_count can only run once in one proc
	 */
	long i, j, blk_cnt;
	get_file_count(jo->path);
	p_dir = (s_dir *)pthread_getspecific(dir_key);

	if (p_dir->file_count <= 1) {
		//the only file is the dir
		printf("path: %s, p_dir->file_count is %d\n", jo->path, p_dir->file_count);
	}
	if (p_dir->file_count)
		p_dir->files = (s_file *) calloc(p_dir->file_count, sizeof(s_file));
	if (p_dir->total_size)
		p_dir->blocks = (s_block *) calloc(BLOCK_COUNT(p_dir->total_size), sizeof(s_block));
	printf("s_file size: %ld\n", sizeof(s_file));
	printf("file count: %d\n", p_dir->file_count);
	printf("total size: %ld\n", p_dir->total_size);
	printf("block count: %ld\n", BLOCK_COUNT(p_dir->total_size));

	/*
	 *  init the files and blocks, and init the last block
	 */
	nftw(jo->path, init_struct, 50, FTW_PHYS);
	/*
	 *  check the last block
	 */
	blk_cnt = p_dir->total_size ? (BLOCK_COUNT(p_dir->total_size) - 1) : 0;
	jo->files = p_dir->files;
	if(p_dir->total_size) {
		(p_dir->blocks + blk_cnt)->size = BLOCK_SIZE - ((blk_cnt + 1) * BLOCK_SIZE
				- p_dir->total_size);
		for (j = p_dir->file_count - 1, i = p_dir->total_size;
			i - (((p_dir->files + j)->size > 0) ? ((p_dir->files + j)->size) : 0) > blk_cnt* BLOCK_SIZE && j >= 0;
			i -= ((p_dir->files + j)->size > 0) ? ((p_dir->files + j)->size) : 0, j--) {
			//printf("start_f: %d, off: %d\n", j, i);
		}
		if (((p_dir->blocks + blk_cnt)->start_f != j) || ((p_dir->blocks + blk_cnt)->start_off
				!= (blk_cnt) * BLOCK_SIZE - (i - (p_dir->files + j)->size))) {
			perror("file division error!");
		}
		jo->blocks = p_dir->blocks;
		/*
		 *  generate MD5 for every block
		 */
		/*
		 for (i=0; i<=blk_cnt; i++)
		 {
		 if (init_block_md5(i))
		 {
		 perror("generate blocks MD5 error!");
		 }
		 }
		 */

		/*
		 *  generate fnv hash for every block
		 */
		/*buf = (unsigned char *) malloc(BLOCK_SIZE);
		if (buf == NULL) {
			perror("malloc BLOCK_SIZE buf failed");
			return -1;
		}
		for (i = 0; i <= blk_cnt; i++) {
			if (fnv_hash_block(jo, i, buf) < 0) {
				perror("generate blocks fnv hash error!");
			}
		}
		free(buf);
		*/
		for (i = 0; i < p_dir->file_count; i++) {
			printf("0%3o\t\t%lld\t\t\t%s\t%s\t", (p_dir->files + i)->mode & 0777,
					(p_dir->files + i)->size, (p_dir->files + i)->name, (p_dir->files + i)->sympath);
			for (j = 0; j < 16; j++)
				printf("%02x", (p_dir->files + i)->md5[j]);
			printf("\n");

		}
		/*
		 *  generate MD5 for every file
		 */
		 /*
		for (i = 0; i < p_dir->file_count; i++) {
			if ((files + i)->size >= 0) {
				if (init_file_md5(jo, i)) {
					perror("generate files MD5 error!");
				}
			}
		}
		*/
	}
	/*
	 *  init the files_size blocks_size
	 */
	jo->files_size = p_dir->file_count * sizeof(s_file);
	jo->blocks_size = BLOCK_COUNT(p_dir->total_size) * sizeof(s_block);
	jo->block_count = BLOCK_COUNT(p_dir->total_size);
	jo->file_count = p_dir->file_count;
	if (p_dir->total_size)
		fnv_hash_all(jo, BLOCK_COUNT(p_dir->total_size));

	for (i = 0; i < p_dir->file_count; i++) {
		printf("0%3o\t\t%lld\t\t\t%s\t%s\t", (p_dir->files + i)->mode & 0777,
				(p_dir->files + i)->size, (p_dir->files + i)->name, (p_dir->files + i)->sympath);
		for (j = 0; j < 16; j++)
			printf("%02x", (p_dir->files + i)->md5[j]);
		printf("\n");

	}
	/*
	 for (i=0; i<=blk_cnt; i++)
	 {
	 //init the block.done
	 (blocks+i)->done = 1;
	 printf("%ld : ", i);
	 for( j = 0; j < 16; j++ )
	 printf( "%02x", (blocks+i)->md5[j] );
	 printf("\n");

	 }
	 */
	for (i = 0; i < BLOCK_COUNT(p_dir->total_size); i++) {
		//init the block.done
		(p_dir->blocks + i)->done = 1;
		//printf("%ld : %u\n", i, (blocks + i)->digest);
	}

	return 0;
}
