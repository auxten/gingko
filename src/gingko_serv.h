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
 * @author auxten  <auxtenwpc@gmail.com>
 * @date 2011-8-1
 **/
int erase_job(std::string &uri_string);

#endif /* GINGKO_SERV_H_ */
