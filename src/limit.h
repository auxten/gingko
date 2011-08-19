/**
 * limit.h
 *
 *  Created on: 2011-8-4
 *      Author: auxten
 **/

#ifndef LIMIT_H_
#define LIMIT_H_

/// limit download rate
void bw_down_limit(int amount, int limit_rate);
/// limit upload rate
void bw_up_limit(int amount, int limit_rate);
/// limit make seed rate
void mk_seed_limit(int amount, int limit_rate);

#endif /** LIMIT_H_ **/
