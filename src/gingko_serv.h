/*
 * gingko_serv.h
 *
 *  Created on: 2011-8-15
 *      Author: auxten
 */

#ifndef GINGKO_SERV_H_
#define GINGKO_SERV_H_

#include <string.h>


/**
 * @brief erase job related stuff only for server
 *
 * @see s_job_t struct
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int erase_job(std::string &uri_string);
/**
 * @brief send the NEWW to all related clients
 *
 * @see
 * @note
 * @author auxten <wangpengcheng01@baidu.com> <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int broadcast_join(s_host_t * host_array, s_host_t *h);


#endif /* GINGKO_SERV_H_ */
