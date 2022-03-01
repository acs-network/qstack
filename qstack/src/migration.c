 /**
 * @file migration.c
 * @brief migration for flow load balance
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.6.30
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.6.30
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif
/*----------------------------------------------------------------------------*/
#include "migration.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
/******************************************************************************/
/* functions */
#if 0
int
mig_flow_group(uint16_t group_id, uint8_t local_core_id, 
		uint8_t remote_core_id)
{
	flow_group_t fg = get_fg_from_fgid(group_id);
	fg->stack_id = remote_core_id;
	if (remote_core_id != fg->orig_stackid) {
		get_stack_from_fgid(remote_core_id)->mig_in_flag = 1;
	}
}

int
mig_item(uint8_t target_core_id, uint8_t local_core_id, void* item)
{
	qstack_t dest_stack = get_stack_context(target_core_id);
	migq_t target_q = dest_stack->mig_queue;
	n21q_enqueue(&target_q->mig_in_queue, local_core_id, item);
	_mm_sfence();
	target_q->mig_ready = 1;
}

void*
__mig_in_check(migq_t q)
{
	void *ret = NULL;
	ret = n21q_dequeue(q);
	if (!ret) {
		q->mig_ready = 0;
		_mm_sfence();
		ret = n21q_dequeue_strong(q);
		if (ret) {
			q->mig_ready = 1;
		}
	}
	return ret;
}
#endif
/******************************************************************************/
/*----------------------------------------------------------------------------*/
