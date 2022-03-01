/**
 * @file migration.h
 * @brief funtions and structs for flow loadbalance migration
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.6.29
 * @version 1.0
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */ 
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __MIGRATION_H_
#define __MIGRATION_H_
/******************************************************************************/
#include "flow_group.h"
/******************************************************************************/
/* global macros */
/*----------------------------------------------------------------------------*/
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
struct 	mig_queue
{
	volatile uint8_t mig_in_flag;	// if the stack have any flow to mig in
	volatile uint8_t mig_ready;		// if there is any data to receive
	struct n21_queue mig_in_queue;	///< datas added by remote core
};
typedef struct mig_queue *migq_t;
/******************************************************************************/
/* function declarations */
// local functions
/**
 * check the mig_in_queue
 *
 * @param q		target mig_queue
 *
 * @return
 * 	return the item get from mig_in_quuee
 * @note
 * 	usually happens just right after flow group migration happens, so the 
 * 	performance is not very important
 */
void*
__mig_in_check(migq_t q);
/******************************************************************************/
/**
 * check if any data from remote core
 *
 * @param q		target mig_queue
 *
 * @return
 * 	return mbuf or stream if any thing in the mig queue; otherwise return NULL
 */
static inline void*
mig_in_check(migq_t q)
{
	if (!q->mig_in_flag || !q->mig_ready) {
		return NULL;
	} else {
		__mig_in_check(q);
	}
}

/**
 * add an item to target remote mig_queue
 *
 * @param target_core_id	target remote mig_queue
 * @param local_core_id		local qstack id
 * @param item				target item
 *
 * @return
 * 	return SUCCESS if add successfully; othrewise return FALSE or FAILED
 * @note
 * 	usually happens just right after flow group migration happens, so the 
 * 	performance is not very important
 */
int
mig_item(uint8_t target_core_id, uint8_t local_core_id, void* item);

/**
 * migrate a flow group to remote core
 *
 * @param group_id			id of local flow group
 * @param local_core_id		id of the core the flow migrates from
 * @param remote_core_id	id of target remote core the flow migrates to
 *
 * @return
 * 	return SUCCESS if success; otherwise return FALSE or FAILED
 *
 * @mote
 * 	this function should always be called by core 0
 */
int
mig_flow_group(uint16_t group_id, uint8_t local_core_id, 
		uint8_t remote_core_id);
/******************************************************************************/
#endif //#ifdef __MIGRATION_H_
