 /**
 * @file flow_group.c
 * @brief devide flows to groups for load-balance
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.6.26
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.6.26
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
#include "flow_group.h"
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
void
fg_table_init(fg_table_t table)
{
	int i;
	flow_group_t fg = table->table;
	systs_t cur_ts = get_sys_ts();
	for (i=0; i<NUM_FLOW_GROUP; i++, fg++) {
		fg->fg_id = i;
		fg->orig_stackid = cal_orig_stackid_from_fgid(i);
		fg->stack_id = fg->orig_stackid;
		fg->last_ts = cur_ts;
		fg->packet_in = 0;
		fg->load = 0;
	}
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
