 /**
 * @file timestamp.c
 * @brief timestamps to estimate processing delays
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.11.25
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.11.25
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
#include "timestamp.h"
#include "tcp_stream.h"
/******************************************************************************/
struct single_timestamp single_ts[MAX_CORE_NUM] = {0};
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
#if REQ_STAGE_TIMESTAMP
void
rs_ts_pass_to_stream(mbuf_t mbuf, struct tcp_stream *cur_stream)
{
	if (mbuf->q_ts) {
		cur_stream->req_stage_ts = mbuf->q_ts;
		mbuf->q_ts = NULL;
	}
}

void
rs_ts_pass_from_stream(struct tcp_stream *cur_stream, mbuf_t mbuf)
{
	if (cur_stream->req_stage_ts) {
		mbuf->q_ts = cur_stream->req_stage_ts;
		cur_stream->req_stage_ts = NULL;
	} else {
		mbuf->q_ts = NULL;
	}
}

rs_ts_t
rs_ts_get_from_sock(int sockid)
{
	tcp_stream_t cur_stream;
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return NULL;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	if (!cur_stream) {
		TRACE_EXCP("no available stream in socket!");
		errno = EBADF;
		return NULL;
	}
	return cur_stream->req_stage_ts;
}
#endif
/******************************************************************************/
/*----------------------------------------------------------------------------*/
