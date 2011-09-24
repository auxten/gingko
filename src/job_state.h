/**
 * job_state.h
 *
 *  Created on: 2011-7-15
 *      Author: auxten
 **/

#ifndef JOB_STATE_H_
#define JOB_STATE_H_

/// job init succeed
static const unsigned int   JOB_SUCC =                  101;
/// job init failed due to unsupported file type
static const unsigned int   JOB_FILE_TYPE_ERR =         102;
/// job init failed due to can't open the file
static const unsigned int   JOB_FILE_OPEN_ERR =         103;
/// job init failed due to recurse error
static const unsigned int   JOB_RECURSE_ERR =           104;
/// job is going to be erased
static const unsigned int   JOB_TO_BE_ERASED =          105;
/// job is joined, ie: g_job is ready
static const unsigned int   JOB_IS_JOINED =             106;
/// job init failed due to recurse error
static const unsigned int   JOB_RECURSE_DONE =          107;


#endif /** JOB_STATE_H_ **/
