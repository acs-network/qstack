/**
 * @file flow_group.h
 * @brief devide flows to groups for load-balance
 * @author Shen Yifan (shenyifan@cit.ac.cn)
 * @date 2019.6.26
 * @version 1.0
 * @detail Function list: \n
 *   1. get_fgid_from_fg(): get fgid of target flow  group\n
 *   2. get_fgid_from_port(): get fgid the port belongs to\n
 *   3. get_fgid_from_stream(): get fgid the tcp stream belongs to\n
 *   4. get_fg_from_fgid(): get the flow group with target id\n
 *   5. get_fg_from_port(): get the flow group te port belongs to\n
 *   6. get_fg_from_stream(): get he flow group the tcp stream belongs to\n
 *   7. get_stackid_from_fg(): get the stackid of the qstack the fg currently 
 *   			belongs to\n
 *   8. get_stackid_from_fgid(): get the stackid of the qstack the fgid of the 
 *   		flow group currently belongs to\n
 *   9. get_stackid_from_port(): get the stackid of the qsatck the port 
 *   			currently belongs to\n
 *   10. get_stackid_from_stream(): get the stackid of the qstack the tcp 
 *   			stream currently belongs to
 *   11. get_stack_from_fg(): get the qstack the fg currently belongs to\n
 *   12. get_stack_from_fgid(): get the qstack the fgid currently belongs to\n
 *   13. get_stack_from_port(): get the qstack the port cueerntly belongs to\n
 *   14. get_stack_from_stream(): get the qstack the tcp stream cueerntly
 *   			belongs to\n
 *   15. fg_load_recored(): increase the load statistic of the flow group 
 *   			after receiving packet
 *   16. get_orig_stackid_from_fg(): get the stackid of the the original 
 *   			qstack the flow group belongs to
 *   17. cal_orig_stackid_from_fgid(): calculate the stackid of the original 
 *   			qstack by the flow_group_id
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
#ifndef __FLOW_GROUP_H_
#define __FLOW_GROUP_H_
/******************************************************************************/
#include "tcp_stream.h"
/******************************************************************************/
/* global macros */
#define NUM_FLOW_GROUP		2048
#define GROUP_LOAD_CYCLE	1000	/// update group load every 1000 ms
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
struct flow_group_entry
{
	uint8_t fg_id;
	uint8_t orig_stackid; ///< stack_id of the original stack the fg belongs to
	uint8_t stack_id;	///< stack_id of the stack context the fg belongs to
	uint32_t last_ts;	///< last time updated load (in ms)
	volatile uint32_t packet_in;	/** num of packets received this period up 
									  to now */
	uint32_t load;		///< packets processed every period smoothed by EWMA
};
struct flow_group_table
{
	struct flow_group_entry table[NUM_FLOW_GROUP];
};// TODO: cacheline aligned
typedef struct flow_group_entry *flow_group_t;
typedef struct flow_group_table *fg_table_t;
/******************************************************************************/
/* function declarations */
// get flow group id
/**
 * get flow_group_id of given flow group
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the flow group id
 */
static inline uint16_t
get_fgid_from_fg(flow_group_t flow_group)
{
	return flow_group->fg_id;
}

/**
 * calculate the flow_group_id of the given port
 *
 * @param port	target tcp port (in network order)
 *
 * @return
 * 	the flow group id
 */
static inline uint16_t
get_fgid_from_port(uint16_t port)
{
	return port & 0x0fff;
}

/**
 * get flow_group_id of cirten tcp stream
 *
 * @param cur_stream	target tcp stream
 *
 * @return
 * 	the flow group id
 */
static inline uint16_t
get_fgid_from_stream(tcp_stream_t cur_stream)
{
	return get_port_group_id(cur_stream->dport);
}
/*----------------------------------------------------------------------------*/
// get flow group entry
/**
 * get flow group entry with flow_group_id
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the flow group 
 */
static inline flow_group_t
get_fg_from_fgid(uint16_t fg_id)
{
//	return &get_global_ctx()->fg_table->table[fg_id];
}
	
/**
 * get flow group entry the port belongs to
 *
 * @param port	target tcp port
 *
 * @return
 * 	the flow group 
 */
static inline flow_group_t
get_fg_from_port(uint16_t port)
{
	return get_fg_from_id(get_fgid_from_port(port));
}

/**
 * get flow group entry the stream belongs to
 *
 * @param cur_stream	target tcp stream
 *
 * @return
 * 	the flow group 
 */
static inline flow_group_t
get_fg_from_stream(tcp_stream_t cur_stream)
{
	return get_fg_from_id(get_fgid_from_stream(cur_stream));
}
/*----------------------------------------------------------------------------*/
// get the stack_id of the stack context the flow group belongs to
/**
 * get stack_id of the qstack the flow group currently belongs to
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the qstack id
 */
static inline uint8_t
get_stackid_from_fg(flow_group_t flow_group)
{
	return flow_group->stack_id;
}

/**
 * get stack_id of the qstack the flow group with given id currently belongs to
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the qstack id
 */
static inline uint8_t
get_stackid_from_fgid(uint16_t group_id)
{
	return get_stackid_from_fg(get_fg_from_fgid(group_id));
}

/**
 * get stack_id of the qstack the port currently belongs to
 *
 * @param port	target tcp port
 *
 * @return
 * 	the qstack id 
 */
static inline uint8_t
get_stackid_from_port(uint16_t port)
{
	return get_stackid_from_fg(get_fg_from_port(port));
}

/**
 * get stack_id of the qstack the stream currently belongs to
 *
 * @param cur_stream	target tcp stream
 *
 * @return
 * 	the qstack id 
 */
static inline uint8_t
get_stackid_from_stream(tcp_stream_t cur_stream)
{
	return get_stackid_from_fgid(get_fgid_from_stream(cur_stream));
}
/*----------------------------------------------------------------------------*/
// get the stack context the flow group belongs to
/**
 * get stack context of the qstack the flow group currently belongs to
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the qstack context
 */
static inline qstack_t
get_stack_from_fg(flow_group_t flow_group)
{
	return get_stack_context(get_stackid_from_fg(flow_group));
}

/**
 * get stack context of the qstack the flow group with given id currently belongs to
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the qstack context
 */
static inline qstack_t
get_stack_from_fgid(uint16_t fg_id)
{
	return get_stack_from_fg(get_fg_from_fgid(fg_id));
}

/**
 * get stack context of the qstack the port currently belongs to
 *
 * @param port	target tcp port
 *
 * @return
 * 	the qstack context 
 */
static inline qstack_t
get_stack_from_port(uint16_t port)
{
	return get_stack_from_fg(get_fg_from_port(port));
}

/**
 * get stack context of qstack the stream currently belongs to
 *
 * @param cur_stream	target tcp stream
 *
 * @return
 * 	the qstack context
 *
 * @note
 * 	cur_stream->qstack also point to the stack context it belongs to, but it 
 * 	may be multi-thread unsafe.
 */
static inline qstack_t
get_stack_from_stream(tcp_stream_t cur_stream)
{
	return get_stack_from_fg(get_fg_from_stream(cur_stream));
}
/*----------------------------------------------------------------------------*/
/**
 * increase the load statistic of the flow group after receiving packet
 *
 * @param flow_group	target flow group
 *
 * @return null
 */
static inline void
fg_load_record(flow_group_t flow_group)
{
	flow_group->packet_in++;
}
/******************************************************************************/
/**
 * get the stackid of the the original qstack the flow group belongs to
 *
 * @param flow_group	target flow group
 *
 * @return
 * 	the stack_id of the flow_group's original stack
 */
static inline uint16_t
get_orig_stackid_from_fg(flow_group_t flow_group)
{
	return flow_group->orig_stackid;
}

/**
 * calculate the stackid of the original qstack by the flow_group_id
 *
 * @param fg_id		id of the target flow group
 *
 * @return
 *  the original stack_id calculated by the flow_group_id
 */
static inline uint16_t
cal_orig_stackid_from_fgid(uint16_t fg_id)
{
	// TODO: support for dynamic stack num
	return fg_id % CONFIG.num_stacks;
}
/******************************************************************************/
void
fg_table_init(fg_table_t table);
/*----------------------------------------------------------------------------*/
#endif //#ifdef __FLOW_GROUP_H_
