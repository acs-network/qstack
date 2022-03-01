/*
* mTCP source code is distributed under the Modified BSD Licence.
* 
* Copyright (C) 2015 EunYoung Jeong, Shinae Woo, Muhammad Jamshed, Haewon Jeong, 
* Sunghwan Ihm, Dongsu Han, KyoungSoo Park
* 
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file tcp_stream.c
 * @brief functions to handle tcp streams
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.7.18
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.25
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DETAIL
#endif
#include "tcp_stream.h"
#include "timer.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* local data structures */
char *state_str[] = {
	"TCP_ST_CLOSED", 
	"TCP_ST_LISTEN", 
	"TCP_ST_SYN_SENT", 
	"TCP_ST_SYN_RCVD", 
	"TCP_ST_ESTABILSHED", 
	"TCP_ST_FIN_WAIT_1", 
	"TCP_ST_FIN_WAIT_2", 
	"TCP_ST_CLOSE_WAIT", 
	"TCP_ST_CLOSING", 
	"TCP_ST_LAST_ACK", 
	"TCP_ST_TIME_WAIT"
};
/******************************************************************************/
/* local static functions */
/******************************************************************************/
/* functions */
tcp_stream *
tcp_stream_create(qstack_t qstack, socket_t socket, int type, uint32_t saddr, 
		uint16_t sport, uint32_t daddr, uint16_t dport, uint32_t seq)
{
	tcp_stream *stream = NULL;
	int ret;
	struct ipv4_5tuple qtuple = {0};
	uint8_t *sa;
	uint8_t *da;
	
	TRACE_CHECKP("try to create new tcp stream!\n");
	stream = (tcp_stream_t)mempool_alloc_chunk(get_global_ctx()->mp_stream, 
			qstack->stack_id);
	if (!stream) {
		TRACE_ERROR("failed to alloc tcp stream at core %d!\n", 
				qstack->stack_id);
	}
	// TODO: if NULL
	memset(stream, 0, sizeof(struct tcp_stream));

	BSTAT_ADD(qstack->flow_created, 1);
	stream->id = 0;
	stream->qstack = qstack;
	stream->saddr = saddr;
	stream->sport = sport;
	stream->daddr = daddr;
	stream->dport = dport;
	qtuple.proto = 6;	// TCP protocol
	qtuple.ip_src = saddr;
	qtuple.ip_dst = daddr;
	qtuple.port_src = sport;
	qtuple.port_dst = dport;

	ret = ht_insert(get_global_ctx()->stream_ht[qstack->stack_id], &qtuple, 
			stream);
	qstack->flow_cnt++;

	if (socket) {
		stream->socket = socket;
		socket->stream = stream;
		stream->id = socket->id;
	}

	stream->state = TCP_ST_LISTEN;

#ifndef TCP_SIMPLE_RTO
	stream->on_rto_idx = -1;
#else
	stream->on_rto_idx = 0;
#endif
	stream->priority = 0;
	
	
	stream->rcvvar.irs = seq;
	stream->rcvvar.rcv_wnd = TCP_INITIAL_WINDOW;
	stream->rcvvar.snd_wl1 = stream->rcvvar.irs - 1;
	rb_init(qstack, &stream->rcvvar.rcvbuf, stream->rcvvar.irs + 1);
	
	stream->sndvar.ip_id = 0;
	stream->sndvar.mss = TCP_DEFAULT_MSS;
	stream->sndvar.eff_mss = stream->sndvar.mss;
	stream->sndvar.wscale_mine = TCP_DEFAULT_WSCALE;
	stream->sndvar.wscale_peer = 0;
//	stream->sndvar.nif_out = get_output_interface(stream->daddr);
	stream->sndvar.nif_out = 0;
	stream->sndvar.iss = (uint32_t)rand();
	stream->sndvar.snd_una = stream->sndvar.iss;
	stream->sndvar.snd_wnd = TCP_INIT_CWND;
	stream->sndvar.rto = TCP_INITIAL_RTO;
	sb_init(qstack, &stream->sndvar.sndbuf, stream->sndvar.iss + 1);

	stream->rcv_nxt = stream->rcvvar.irs;
	stream->snd_nxt = stream->sndvar.iss;
	stream->if_req_head = NULL;
	stream->if_req_high = NULL;
	event_list_init(&stream->pending_events);
#if REQ_STAGE_TIMESTAMP
	stream->req_stage_ts = NULL;
#endif
	event_list_init(&stream->pending_events);

	TRACE_CNCT("new stream created! @ Stream %d\n", stream->id);
	return stream;
}

int
tcp_stream_close(int core_id, tcp_stream_t cur_stream)
{
	qstack_t qstack = cur_stream->qstack;

	if (cur_stream->closed) {
		TRACE_EXCP("try to close an already closed stream @ Stream %d\n", 
				cur_stream->id);
		return 0;
	}
	cur_stream->closed = TRUE;
	// the socket should be freed at stack thread
//	cur_stream->socket->socktype = SOCK_TYPE_UNUSED;
//	cur_stream->socket = NULL;

	if (cur_stream->state != TCP_ST_ESTABLISHED && 
			cur_stream->state != TCP_ST_CLOSE_WAIT) {
		TRACE_EXCP("Stream %d closed at unexcepted state %d\n", 
				cur_stream->id, cur_stream->state);
		errno = EBADF;
		return -1;
	}
	TRACE_CLOSE("close call is enqueued @ Stream %d @ Stack %d", 
			cur_stream->id, qstack->stack_id);
	streamq_enqueue(&qstack->close_queue, core_id, cur_stream);
	cur_stream->sndvar.on_closeq = TRUE;
	
	return 0;
}

//TODO: desgroy the stream
void
tcp_stream_destroy(qstack_t qstack, tcp_stream *cur_stream)
{
	// TODO: when destroying the stream, state and buffer access may be unsafe
	struct ipv4_5tuple qtuple = {0};
	int ret;
	
	TRACE_CHECKP("tcp_stream_destroy() @ Stream %d "
			"reason:%d, src_port: %d, src_ip:%d.%d\n", 
			cur_stream->id, cur_stream->close_reason, ntohs(cur_stream->dport), 
			(cur_stream->daddr&0xff0000)>>16, (cur_stream->daddr>>24));
	// TODO: remove the stream from queues?	
	if (cur_stream->on_timeout_list) {
		timeout_list_remove(qstack, cur_stream);
	}
	if (cur_stream->on_timewait_list) {
		timewait_list_remove(qstack, cur_stream);
	}
#ifndef TCP_SIMPLE_RTO
	if (cur_stream->on_rto_idx >= 0) 
#else 
	if (cur_stream->on_rto_idx) 
#endif
	{
		rto_list_remove(qstack, cur_stream);
	}
	/* the state should always have been changed to CLOSED when this function 
	 * is called */
//	cur_stream->state = TCP_ST_CLOSED;

	qtuple.proto = 6;	// TCP protocol
	qtuple.ip_src = cur_stream->saddr;
	qtuple.ip_dst = cur_stream->daddr;
	qtuple.port_src = cur_stream->sport;
	qtuple.port_dst = cur_stream->dport;
	// TODO: multi-core mode
	ret =ht_delete(get_global_ctx()->stream_ht[qstack->stack_id], &qtuple);
	if (ret != 1) {
		TRACE_EXCP("failed to delete from htable @ Stream %d\n", 
				cur_stream->id);
	}
	qstack->flow_cnt--;
	
	// clear the send buff
	sb_free(qstack, &cur_stream->sndvar.sndbuf);
	// clear the recv buff
	rb_clear(qstack, &cur_stream->rcvvar.rcvbuf);
	
	// free the socket
	if (cur_stream->socket) {
		// TODO: multi-core mode
		socket_free(qstack->stack_id, cur_stream->socket->id);
	}
	mempool_free_chunk(get_global_ctx()->mp_stream, (void*)cur_stream, 
			qstack->stack_id);
	TRACE_CLOSE("tcp stream closed @ Stream %d\n", cur_stream->id);
	DSTAT_ADD(qstack->flow_closed, 1);
}

// TODO NOW:inline
void
print_stream_sndvar(tcp_stream_t cur_stream)
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	snd_buff_t buff = &sndvar->sndbuf;
	TRACE_STREAM("snd_nxt: %u, snd_una: %u, buff head: %u, buff next: %u\n", 
			cur_stream->snd_nxt, sndvar->snd_una, buff->head_seq, 
			buff->next_seq);
}

char *
tcp_state_to_string(const tcp_stream_t cur_stream)
{
	return state_str[cur_stream->state];
}
/******************************************************************************/
