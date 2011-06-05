/*
 * route.h
 *
 *  Created on: 2011-5-25
 *      Author: auxten
 */

#ifndef ROUTE_H_
#define ROUTE_H_

int get_blk_src(s_job * jo, int src_max, int64_t blk_num,
        vector<s_host> * h_vec);
int decide_src(s_job * jo, int src_max, int64_t blk_num,
        vector<s_host> * h_vec, s_host * h);

#endif /* ROUTE_H_ */
