/**
 * snap.h
 *
 *  Created on: 2011-7-14
 *      Author: auxten
 **/

#ifndef SNAP_H_
#define SNAP_H_

/// dump the progress of this blk
void dump_progress(s_job_t * jo, s_block_t * blk);
/// examine the snap file handled by fd, if OK load it
int load_snap(s_job_t * jo);

#endif /** SNAP_H_ **/
