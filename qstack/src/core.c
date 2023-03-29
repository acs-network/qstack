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
/*
 * QStack The main body of the QStack, indluding the system architecture, 
 * system initialization, scheduling, network stack processing, user APIs, etc.
 *
 * Auther  Yifan Shen
 */
/**
 * @file core.c
 * @brief mainloop and core functions
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.21
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.30
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
#include "qstack.h"
#include "timestamp.h"
#include "dpdk_module.h"
#include "eth_in.h"
#include "timer.h"
#include "runtime_mgt.h"
#include "message.h"

#include <pthread.h>  
#include <stdio.h>  
#include <sys/types.h>  
#include <sys/syscall.h> 
#include "api.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* glbal variables */
g_ctx_t g_qstack;
struct q_statistic q_stat;
struct config CONFIG = {0};
ts_t ts_system_init;
#if STAGE_TIMEOUT_TEST_MODE
uint64_t last_recv_check_ts = 0;
uint32_t last_drop_num = 0;
#endif
FILE *fp_screen;
unsigned char core_map[MAX_CORE_NUM] = {0};
/******************************************************************************/
/* local static functions */
/* functional functions */

static inline int
recv_check_timeout_test(qstack_t qstack, int stage_num)
{
#if STAGE_TIMEOUT_TEST_MODE
	//TODO: start check after the stack begin to receive packets
	uint64_t interval = stage_timeout_test(&last_recv_check_ts, 
			RECV_CHECK_TIMEOUT_THRESH);
	uint32_t drop_num = io_get_rx_err(0);
	if (drop_num != last_drop_num) {
		TRACE_EXCP(qstack, "NIC drop detect! "
				"drop num: %u, interval: %llu, ts:%llu\n", 
				drop_num, interval, last_recv_check_ts);
		last_drop_num = drop_num;
		return TRUE;
	}
#endif
	return FALSE;
}

static inline void
handle_close_call(qstack_t qstack, tcp_stream_t cur_stream)
{
	struct tcp_send_vars *sndvar;
	// handle q_close() call from application
	TRACE_CLOSE("process close call @ Stream %d\n", cur_stream->id);
	sndvar = &cur_stream->sndvar;
	sndvar->fss = sndvar->sndbuf.next_seq;

	if (cur_stream->have_reset) {
		if (cur_stream->state != TCP_ST_CLOSED) {
			cur_stream->close_reason = TCP_RESET;
			cur_stream->state = TCP_ST_CLOSED;
			TRACE_CLOSE("close because of RESET call @ Stream %d\n", 
					cur_stream->id);
			TRACE_STATE("@ Stream %d: trasform to TCP_ST_CLOSED\n", 
					cur_stream->id);
			tcp_stream_destroy(qstack, cur_stream);
		} else {
			TRACE_EXCP("Stream already closed.\n");
		}
	} else if (cur_stream->state != TCP_ST_CLOSED) {
		if (cur_stream->state == TCP_ST_ESTABLISHED) {
			cur_stream->state = TCP_ST_FIN_WAIT_1;
			TRACE_STATE("@ Stream %d: from TCP_ST_ESTABLISHED to "
					"TCP_ST_FIN_WAIT_1\n", 
					cur_stream->id);
		} else if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
			cur_stream->state = TCP_ST_LAST_ACK;
			TRACE_STATE("@ Stream %d: from TCP_ST_CLOSE_WAIT to "
					"TCP_ST_LAST_ACK\n", 
					cur_stream->id);
		}
		// TODO: if the stream has already been on control_queue, it may 
		// cause duplicate FIN
		control_queue_add(qstack, cur_stream);
	} else {
		TRACE_ERR("Already closed connection, not excepted branch\n");
	}
}

static inline void
handle_connect_call(qstack_t qstack, socket_t socket)
{
	tcp_stream_t cur_stream = NULL;
	TRACE_CNCT("handle connect call at stack @ Socket %d\n", socket->id);
	if (socket->saddr.sin_port == INPORT_ANY ||
		socket->saddr.sin_addr.s_addr == INADDR_ANY) {
		// connect without binding address
		TRACE_TODO();
	} else {
		// connect with binded address
		/* the cur_stream validation will be checked in tcp_stream_create() */
		cur_stream = tcp_stream_create(qstack, socket, 0, 
				socket->saddr.sin_addr.s_addr, socket->saddr.sin_port, 
				socket->daddr.sin_addr.s_addr, socket->daddr.sin_port, 0);
		/* init congestion control */
		cur_stream->sndvar.cwnd = 1;
		cur_stream->sndvar.ssthresh = cur_stream->sndvar.mss * 10;
		/* look up arp table and get dest MAC address */
		get_dst_hwaddr(socket->daddr.sin_addr.s_addr, cur_stream->dhw_addr);
		/* updata tcp state machine and prepare to send SYN */
		cur_stream->state = TCP_ST_SYN_SENT;
		TRACE_STATE("@ Stream %d: from TCP_ST_LISTEN to TCP_ST_SYN_SENT\n", 
				cur_stream->id);
		
		control_queue_add(qstack, cur_stream);
	}
}

static inline void
handle_app_call(qstack_t qstack, uint32_t cur_ts)
{
	tcp_stream_t cur_stream;
	stream_queue_t close_queue = &qstack->close_queue;
	stream_queue_t connect_queue = &qstack->connect_queue;
	int cnt = 0;

	while (cnt < MAX_CLOSE_BATCH && 
			(cur_stream = streamq_dequeue(close_queue)) != NULL) {
		handle_close_call(qstack, cur_stream);
		cnt++;
	}
	cnt = 0;

	while (cnt < MAX_CONNECT_BATCH && 
			(cur_stream = streamq_dequeue(connect_queue)) != NULL) {
		handle_connect_call(qstack, (socket_t)cur_stream);
		cnt++;
	}
}
/******************************************************************************/
static inline void
global_self_check()
{
	if (sizeof(struct rte_mbuf) > 128) {
		TRACE_ERROR("the mbuf size is %d!\n", sizeof(struct rte_mbuf));
	}
}

static inline void
qcore_init(qcore_t core, int core_id)
{
	core->core_id = core_id;
	// TODO: rt_ctx should be allocated before qcore_init
	core->rt_ctx = (rtctx_t)calloc(1, sizeof(struct runtime_ctx));
	runtime_init(core->rt_ctx, core_id);
}

static inline void
gctx_init()
{
	int i, csize;
	TRACE_CHECKP("start gctx_init()\n");
	g_qstack = (g_ctx_t)calloc(1, sizeof(struct qstack_context_global));

	g_ctx_t gctx = get_global_ctx();
    printf("g_qstack address is %p and gctx address is %p \n",g_qstack,gctx);	
	for (i=0; i<MAX_CORE_NUM+1; i++) {
		qcore_t qcore = (qcore_t)calloc(1, sizeof(struct qcore_context));
		gctx->core_contexts[i] = qcore;
		qcore_init(qcore, i);
	}

	gctx->listeners = listeners_init();
    printf("gctx->listeners address is %p \n",gctx->listeners);
	
    gctx->socket_table = socket_table_init();


    printf("gctx-socket_table address is %p \n",gctx->socket_table);

	for (i=0; i<CONFIG.num_stacks; i++) {
		gctx->stream_ht[i] = ht_create(MAX_FLOW_PSTACK+i);
		TRACE_PROC(" gctx->stream_ht[i] is %p \n", gctx->stream_ht[i]);
	}
	// TODO: MAX_LISTENER_NUM should be configed dynamically
	gctx->listener_ht = ht_create(10);

	// the chunk num should be dynamically
	TRACE_CHECKP("begin to create global mempools\n");
	csize = sizeof(struct tcp_listener);
	gctx->mp_listener = mempool_create(csize, 10, 10, 
			CONFIG.num_stacks);
	printf("gctx->mp_listener %x \n",gctx->mp_listener);
	if (gctx->mp_listener) {
		TRACE_MEMORY("mp_listener create done, "
				"chunk_size: %d, chunk_num: %d, mp_num: %d\n",
				csize, 10, CONFIG.num_stacks);
	} else {
		TRACE_ERR("mp_listener create failed!, "
				"chunk_size: %d, chunk_num: %d, mp_num: %d\n",
				csize, 10, CONFIG.num_stacks);
	}
	csize = sizeof(struct tcp_stream);
	gctx->mp_stream = mempool_create(csize, MAX_FLOW_PSTACK, 
			0, CONFIG.num_stacks); 
	if (gctx->mp_stream) {
		TRACE_MEMORY("mp_stream create done, "
				"chunk_size: %d, chunk_num: %d, mp_num: %d+1\n",
				csize, MAX_FLOW_PSTACK, CONFIG.num_stacks);
	} else {
		TRACE_ERR("mp_stream create failed!, "
				"chunk_size: %d, chunk_num: %d, mp_num: %d+1\n",
				csize, MAX_FLOW_PSTACK, CONFIG.num_stacks);
	}
}

/** alloc and init a sender context */
static inline sender_t
sender_init()
{
	sender_t sender = (sender_t)calloc(1, sizeof(struct sender_context));
	sstreamq_init(&sender->control_queue, MAX_FLOW_NUM>>(2+MEM_SCALE));
//	sstreamq_init(&sender->send_queue, CONFIG.num_stacks, MAX_FLOW_NUM);
	sstreamq_init(&sender->ack_queue, MAX_FLOW_NUM>>(2+MEM_SCALE));
	return sender;
}

static inline qstack_t
stack_context_init(int stack_id)
{ 
	TRACE_CHECKP("start init stack thread %d\n", stack_id);
	qstack_t qstack = (qstack_t)calloc(1, sizeof(struct qstack_context));
	qstack->stack_id = stack_id;
	qstack->flow_cnt = 0;
	qcore_t qcore = get_core_context(stack_id);
	
	TRACE_CHECKP("start stack_context_init() @ Stack %d\n", stack_id);
	get_global_ctx()->stack_contexts[stack_id] = qstack;
	qcore->rt_ctx->qstack = qstack;
    qstack->rt_ctx = qcore->rt_ctx;

	sender_t sender = sender_init();
	qstack->sender = sender;
	streamq_init(&qstack->send_event_queue, CONFIG.num_cores, 
			MAX_FLOW_NUM>>(2+MEM_SCALE));
	streamq_init(&qstack->send_hevent_queue, CONFIG.num_cores, 
			MAX_FLOW_NUM>>(2+MEM_SCALE));
	streamq_init(&qstack->close_queue, CONFIG.num_cores, 
			MAX_FLOW_NUM>>(2+MEM_SCALE));
	streamq_init(&qstack->connect_queue, CONFIG.num_cores, 
			MAX_FLOW_NUM);
	qstack->cpu = qstack->stack_id;
//	qstack->queue = 0;
	timer_init(qstack);

#ifdef PTHREAD_SCHEDULING
	pthread_mutex_init(&qstack->rt_ctx->wakeup_ctx.epoll_lock, NULL);
	pthread_cond_init(&qstack->rt_ctx->wakeup_ctx.epoll_cond, NULL);
#endif
	
	mbufq_mp_init(&qstack->mp_mbufq, MAX_FLOW_NUM*4, CONFIG.sndbuf_size);
	memset(&qstack->req_stage_ts, 0, sizeof(struct req_stage_timestamp));
	memset(&qstack->mloop_ts, 0, sizeof(struct mainloop_timestamp));
	return qstack;
}

static inline void 
wakeup_application(qstack_t qstack)
{
#if CONTEXT_SWITCH
	struct thread_wakeup *ctx = &qstack->rt_ctx->wakeup_ctx;
	if (ctx->knocked) {
		DSTAT_ADD(qstack->wakeup_num, 1);
		TRACE_THREAD("try to wake up application thread @ Core %d\n", 
				qstack->stack_id);
	#ifdef PTHREAD_SCHEDULING
		pthread_mutex_lock(&ctx->epoll_lock);
		pthread_cond_signal(&ctx->epoll_cond);
		pthread_mutex_unlock(&ctx->epoll_lock);
	#endif
	#ifdef COROUTINE_SCHEDULING
//		runtime_schedule(qstack->rt_ctx, qstack->rt_ctx->last_stack_ts);
		yield_to_qapp(qstack->rt_ctx, qstack->thread, qstack->rt_ctx->qapp);
		TRACE_THREAD("return from app thread %d to stack thread @ Core %d!\n",
				qstack->rt_ctx->qapp->app_id, qstack->stack_id);
	#endif
	}
#endif	
}
/******************************************************************************/
// state statistic functions
static inline void
print_network_state_pstack(qstack_t qstack)
{
#if STATISTIC_STATE
	int i;
	
	TRACE_SCREEN(
			"stack %d:\n"
	#if STATISTIC_STATE_BASIC
			"packets in:\t\t%16u/%16u\n"
			"packets out:\t\t%16u/%16u\n"
			"l_re recved:\t\t%16u/%16u\n"
			"h_re recved:\t\t%16u/%16u\n"
			"response out:\t\t%16u/%16u\n"
			"flow_created:\t\t%16u\n"
			"rcv_backlog:\t\t%16u/%16u\n"
	#endif
	#if STATISTIC_STATE_DETAIL
			// info of events
			"read event:\t\t%16u\n"
			"read hevent:\t\t%16u\n"
			"event to add:\t\t%16u\n"
			"event is added:\t\t%16u\n"
			
			// info of function calls
			"recv_check call:\t%16u\n"
			"idle_loop:\t\t%16u\n"
//			"recv_check call zero:\t%16u\n"
//			"recv_mbuf called:\t%16u\n"
			"wakeup time:\t\t%16u\n"
			
			// info of packets
			"rx packets:\t\t%16u\n"
			"tx packets:\t\t%16u\n"
			"packets dfreed:\t\t%16u\n"
			"packets retrans:\t%16u\n"
			"request dropped:\t%16u\n"
			"throughput in:\t\t%16llu Mbps\n"
			"throughput out:\t\t%16llu Mbps\n"
	#endif
			"========================================\n"
			, qstack->stack_id
	#if STATISTIC_STATE_BASIC
			, qstack->pkt_in, qstack->pkt_in - qstack->pkt_in_pre
			, qstack->pkt_out, qstack->pkt_out - qstack->pkt_out_pre
			, qstack->lreq_recv_num, qstack->lreq_recv_num - 
					qstack->lreq_recv_pre
			, qstack->hreq_recv_num, qstack->hreq_recv_num - 
					qstack->hreq_recv_pre
			, qstack->resp_num, qstack->resp_num - qstack->resp_pre
			, qstack->flow_created
			, io_get_rx_backlog(qstack), qstack->rcv_backlog_max
	#endif
	#if STATISTIC_STATE_DETAIL
			// info of events
			, qstack->read_event_num
			, qstack->read_hevent_num 
			, qstack->event_to_add
			, qstack->event_added_done

			// info of function calls
			, qstack->recv_check_call
			, qstack->idle_loop
//			, qstack->recv_check_call_zero
//			, qstack->recv_mbuf_call
			, qstack->wakeup_num

			// info of packets
			, qstack->mbuf_rx_num
			, qstack->mbuf_tx_num
			, qstack->mbuf_dfreed
			, qstack->retrans_snd_num
			, qstack->req_dropped
			, (qstack->byte_in - qstack->byte_in_pre) >> 17
			, (qstack->byte_out - qstack->byte_out_pre) >> 17
	#endif
			);

	BSTAT_SET(qstack->rcv_backlog_max, 0);
	BSTAT_SET(qstack->lreq_recv_pre, qstack->lreq_recv_num);
	BSTAT_SET(qstack->hreq_recv_pre, qstack->hreq_recv_num);
	BSTAT_SET(qstack->resp_pre, qstack->resp_num);
	BSTAT_SET(qstack->pkt_in_pre, qstack->pkt_in);
	BSTAT_SET(qstack->pkt_out_pre, qstack->pkt_out);
	DSTAT_SET(qstack->byte_in_pre, qstack->byte_in);
	DSTAT_SET(qstack->byte_out_pre, qstack->byte_out);
	DSTAT_SET(qstack->recv_check_call, 0);
	DSTAT_SET(qstack->idle_loop, 0);
#endif
}

void
__print_network_state()
{
#if STATISTIC_STATE
	int i,j;
	// for per stack thread
	qstack_t qstack;
	uint32_t mbuf_dfreed = 0;
	uint32_t acked_freed = 0;
	uint32_t flow_created = 0;
	uint32_t flow_closed = 0;
	uint32_t req_dropped = 0;
	
	// for per app thread
	uint32_t uwmbuf_alloced = 0;
	uint32_t request_freed = 0;
	uint32_t high_jump = 0;
	uint32_t recv_called_num = 0;
	uint32_t write_called_num = 0;
	uint32_t close_called_num = 0;
	uint32_t accepted_num = 0;
	uint32_t high_event_queue = 0;
	uint32_t low_event_queue = 0;

	uint32_t pkt_in = 0;
	uint32_t pkt_out = 0;
	uint32_t mbuf_rx_num = 0;
	uint32_t mbuf_tx_num = 0;
	uint32_t req_rcvd = 0;
	uint32_t rsp_sent = 0;
	uint32_t rmbuf_get_num = 0;
	uint64_t log_size = 0;
	uint64_t byte_in = 0;
	uint64_t byte_out = 0;
	static uint32_t pkt_in_pre = 0;
	static uint32_t pkt_out_pre = 0;
	static uint32_t mbuf_rx_num_pre = 0;
	static uint32_t mbuf_tx_num_pre = 0;
	static uint32_t req_rcvd_pre = 0;
	static uint32_t rsp_sent_pre = 0;
	static uint64_t log_size_pre = 0;
	static uint64_t byte_in_pre = 0;
	static uint64_t byte_out_pre = 0;

	for (i=0; i<CONFIG.num_stacks; i++) {
		qstack = get_stack_context(i);
		if (!qstack) {
			return;
		}
		print_network_state_pstack(qstack);

	#if STATISTIC_STATE_BASIC
		pkt_in += qstack->pkt_in;
		pkt_out += qstack->pkt_out;
		req_rcvd += qstack->lreq_recv_num + qstack->hreq_recv_num;
		rsp_sent += qstack->resp_num;
		flow_created += qstack->flow_created;
		byte_in += qstack->byte_in;
		byte_out += qstack->byte_out;
	#endif
	#if STATISTIC_STATE_DETAIL
		mbuf_rx_num += qstack->mbuf_rx_num;
		mbuf_tx_num += qstack->mbuf_tx_num;
		flow_closed += qstack->flow_closed;
		mbuf_dfreed += qstack->mbuf_dfreed;
		acked_freed += qstack->acked_freed;
		req_dropped += qstack->req_dropped;
	#endif
	}

	#if STATISTIC_STATE_DETAIL
	for (i=0; i<CONFIG.num_cores; i++) {
		uwmbuf_alloced += get_global_ctx()->uwmbuf_alloced[i];
		request_freed += get_global_ctx()->request_freed[i];
		accepted_num += get_global_ctx()->accepted_num[i];
		high_jump += get_global_ctx()->highjump_num[i];
		rmbuf_get_num += get_global_ctx()->rmbuf_get_num[i];

		recv_called_num += get_global_ctx()->recv_called_num[i];
		write_called_num += get_global_ctx()->write_called_num[i];
		close_called_num += get_global_ctx()->close_called_num[i];

		high_event_queue += get_global_ctx()->high_event_queue[i];
		low_event_queue += get_global_ctx()->low_event_queue[i];
	}
	#endif
	log_size = total_log_size;

	TRACE_SCREEN(
			"global:\n"
	#if STATISTIC_STATE_BASIC
			"==========Throughput:\n"
			"packet_in:\t\t%16u/%16u\n" // io layer info
			"packet_out:\t\t%16u/%16u\n"
			"request_in:\t\t%16u/%16u\n" // tcp layer info
			"response_out:\t\t%16u/%16u\n"
			"throughput in:\t\t%16llu Mbps\n" // io layer info
			"throughput out:\t\t%16llu Mbps\n"
			"==========Stack state:\n"
			"flow created:\t\t%16u\n"
			"NIC recv drop:\t\t%16u\n"
	#endif
	#if STATISTIC_STATE_DETAIL
//			"RX ring mbuf allocation failures :\t\t%16u\n"
//			"NIC recv drop time test:\t\t%16u\n"
//			"NIC tx send:\t\t%16u\n"
//			"NIC tx err:\t\t%16u\n"
			"==========Mbuf alloc and free:\n"
//			"uw alloc time:\t\t%16u\n"
//			"sw alloc time:\t\t%16u\n"
//			"driver total free:\t%16u\n"
//			"driver total recv:\t%16u\n"
//			"rx mbufs:\t\t%16u/%16u\n" // driver layer info
//			"tx mbufs:\t\t%16u/%16u\n"
			"app get mbufs:\t\t%16u\n"
			"request_freed:\t\t%16u\n" // freeed by q_free_mbuf()
			"req_dropped:\t\t%16u\n" // dropped because failed to add to buff
			"mbuf_dfreed:\t\t%16u\n"
			"uwmbuf_alloced:\t\t%16u\n"
			"acked_freed:\t\t%16u\n"
//			"high-req jump:\t\t%16u\n"
			"==========App calls:\n"
			"accepted_num:\t\t%16u\n"
			"q_recv() called:\t%16u\n"
			"q_send() called:\t%16u\n"
			"q_close() called:\t%16u\n"
			"==========Qevent statistic:\n"
			"high event_queuing num:\t%16u\n"
			"low event_queuing num:\t%16u\n"
			"==========Temp statistic:\n"
			"flow closed:\t\t%16u\n"
	#endif
	#if STATISTIC_LOG
			"==========Log statistic:\n"
			"log_size:\t\t%16llu\n"
			"log_rate:\t\t%16llu KB/s\n"
	#endif
			"system time:\t%llu\n"
			"============================================================\n"
	#if STATISTIC_STATE_BASIC
			// throughput
			, pkt_in, pkt_in - pkt_in_pre
			, pkt_out, pkt_out - pkt_out_pre
			, req_rcvd, req_rcvd - req_rcvd_pre
			, rsp_sent, rsp_sent - rsp_sent_pre
			, (byte_in - byte_in_pre) >> 17
			, (byte_out - byte_out_pre) >> 17
			// stack state
			, flow_created
			, io_get_rx_err(0)
	#endif
	#if STATISTIC_STATE_DETAIL
//			, io_get_rx_nobuf_err(0)
//			, io_get_rx_last_time(0)
//			, io_get_tx_out(0)
//			, io_get_tx_err(0)
			
			// mbuf alloc and free
//			, dpdk_total_uwget_num()
//			, dpdk_total_swget_num()
//			, dpdk_total_free_num()
//			, dpdk_total_recv_num()
//			, mbuf_rx_num, mbuf_rx_num - mbuf_rx_num_pre
//			, mbuf_tx_num, mbuf_tx_num - mbuf_tx_num_pre
			, rmbuf_get_num
			, request_freed
			, req_dropped
			, mbuf_dfreed
			, uwmbuf_alloced
			, acked_freed
//			, high_jump

			// app calls
			, accepted_num
			, recv_called_num
			, write_called_num
			, close_called_num

			// event
			, high_event_queue
			, low_event_queue 
			

			// temp variables
			, flow_closed
	#endif
	#if STATISTIC_LOG
			// log
			, log_size
			, (log_size - log_size_pre)/1024
	#endif
			, get_time_s()
			);

//	dpdk_print_state(NULL,0);
	
	pkt_in_pre = pkt_in;
	pkt_out_pre = pkt_out;
	mbuf_rx_num_pre = mbuf_rx_num;
	mbuf_tx_num_pre = mbuf_tx_num;
	req_rcvd_pre = req_rcvd;
	rsp_sent_pre = rsp_sent;
	byte_in_pre = byte_in;
	byte_out_pre = byte_out;
	log_size_pre = log_size;
	#if STATISTIC_TEMP
	for (i=0; i<STAT_TEMP_NUM; i++) {
		uint32_t stat = 0;
		for (j=0; j<CONFIG.num_cores; j++) {
			stat += q_stat.stat_temp[i][j];
		}
		TRACE_SCREEN("stat_temp %d:\t%16d\n", i, stat);
	}
	TRACE_SCREEN("============================================================\n");
	#endif
	#ifdef STATISTIC_FORWARD_BUFF_LEN
	for (i=0; i<CONFIG.num_stacks*WORKER_PER_SERVER; i++) {
		TRACE_SCREEN("redis forward buff %d max: %d\n", 
				i, get_global_ctx()->forward_buff_len[i]);
		get_global_ctx()->forward_buff_len[i] = 0;
	}
	TRACE_SCREEN("============================================================\n");
	#endif
#endif
}

void
immediately_print_netwrok_state()
{
	TRACE_SCREEN("[Crash Information]:\n");
	__print_network_state();
}

void
print_network_state()
{
#if STATISTIC_STATE
	uint64_t cur_ts = get_time_s();

	if (cur_ts - get_global_ctx()->last_print_ts < STATE_PRINT_INTERVAL) {
		return;
	}
	get_global_ctx()->last_print_ts = cur_ts;
	
	__print_network_state();
	socket_scan();
#endif
}

static void
monitor_thread_start()
{
    while (1) {
        print_network_state();
    }
}

static void
msg_thread_start()
{
    while (1) {
        flush_message();
    }
}

static void
log_thread_start()
{
    while (1) {
        flush_log(fp_log);
    }
}
/******************************************************************************/
/* main functions */
static void 
qstack_main_loop(qstack_t qstack)
{
	int rcv_num = 32;
	int snd_num;
	int i;
	int len;
	int ret;
	mbuf_t mbuf;
	systs_t cur_ts = 0;
	systs_t cur_ts_us = 0;
	uint64_t loop_num = 0;
	uint32_t to_send = 0;
	uint32_t ctl_num = 0;
	uint32_t ack_num = 0;
	uint32_t rsp_num = 0;
	uint32_t last_send_check_ts = 0;
	TRACE_LOG("====================\nqstack start at core %d, pid:%d\n", 
			qstack->stack_id, syscall(SYS_gettid));
	//for init before while loop
	DSTAT_SET(qstack->mbuf_dfreed, 0);

	while (1) {
		loop_num++;
		ret = 0;
		TRACE_LOOP("--------------------main loop\n");

//		cur_ts = get_sys_ts();
		cur_ts_us = get_time_us();
		cur_ts = cur_ts_us / 1000;
		qstack->rt_ctx->last_stack_ts = cur_ts_us;
		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_RCHECK);
		rcv_num = io_recv_check(qstack, IFIDX_SINGLE, FETCH_NEW_TS);
		BSTAT_CHECK_SET(rcv_num > qstack->rcv_backlog_max, 
				qstack->rcv_backlog_max, rcv_num);
		rcv_num = MIN(MAX_RECV_BATCH, rcv_num);
		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_RECV);
		for (i=0; i<rcv_num; i++) {
			mbuf= io_recv_mbuf(qstack, IFIDX_SINGLE, &len);
			//TRACE_LOG("mbuf address is %p \n",mbuf);
			process_eth_packet(qstack, IFIDX_SINGLE, cur_ts, mbuf, 0);
			if (mbuf->mbuf_state == MBUF_STATE_RCVED) {
				DSTAT_ADD(qstack->mbuf_dfreed, 1);
				mbuf_set_op(mbuf, MBUF_OP_RCV_SFREE, qstack->stack_id);
				mbuf_free(qstack->stack_id, mbuf);
			}
		}
#if EXTRA_RECV_CHECK >= 1
		ret = io_recv_check(qstack, IFIDX_SINGLE, FETCH_NEW_TS);
		BSTAT_CHECK_SET(rcv_num > qstack->rcv_backlog_max, 
				qstack->rcv_backlog_max, ret);
#endif

		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_TIMTOUT);
		timeout_list_check(qstack, cur_ts);
		timewait_list_check(qstack, cur_ts);
		rto_list_check(qstack, cur_ts);

		if (recv_check_timeout_test(qstack, 1)) {
			TRACE_EXCP("rcv_num while timeout: %d\n", i);
		}

#if EXTRA_RECV_CHECK >= 2
		ret = io_recv_check(qstack, IFIDX_SINGLE, FETCH_NEW_TS);
		BSTAT_CHECK_SET(rcv_num > qstack->rcv_backlog_max, 
				qstack->rcv_backlog_max, ret);
#endif
		// wake up the application thread if in co-work mode
		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_APPTHREAD);
		wakeup_application(qstack);

		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_APPCALL);

#if EXTRA_RECV_CHECK >= 1
		ret = io_recv_check(qstack, IFIDX_SINGLE, FETCH_NEW_TS);
		BSTAT_CHECK_SET(rcv_num > qstack->rcv_backlog_max, 
				qstack->rcv_backlog_max, ret);
#endif
//		snd_num = write_tcp_packets(qstack, ts);
		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_SEND);
		rsp_num = write_tcp_send_queue(qstack, cur_ts);
		handle_app_call(qstack, cur_ts);
		ctl_num = write_tcp_control_queue(qstack, cur_ts);
		rsp_num = write_tcp_send_queue(qstack, cur_ts);
		ack_num = write_tcp_ack_queue(qstack, cur_ts);
		ml_ts_add(&qstack->mloop_ts, MLOOP_ST_SCHECK);
		to_send += ctl_num + ack_num + rsp_num;
#if EXTRA_RECV_CHECK >= 2
		ret = io_recv_check(qstack, IFIDX_SINGLE, FETCH_NEW_TS);
		BSTAT_CHECK_SET(rcv_num > qstack->rcv_backlog_max, 
				qstack->rcv_backlog_max, ret);
#endif
		if (to_send >= 10 || 
				(to_send && (int)(cur_ts_us - last_send_check_ts) > 1) || 
				(int)(cur_ts_us - last_send_check_ts > 10)) {
			if(io_send_check(qstack, IFIDX_SINGLE) == -1){			
				TRACE_EXIT("err packets out\n");
			}
			to_send = 0;
			last_send_check_ts = cur_ts_us;
		}
		DSTAT_CHECK_ADD(!(rcv_num+ctl_num+rsp_num+ack_num), 
				qstack->idle_loop, 1);
		ml_timeout_check(&qstack->mloop_ts, qstack->stack_id, loop_num, 
				rcv_num, ctl_num, rsp_num, ack_num);
#ifdef STACK_ACTIVE_YIELD
		if (!(rcv_num+ctl_num+rsp_num+ack_num) && !qstack->wakeup_ctx.waiting) {
			sched_yield();
		}
#endif
	} // end of mainloop while(1)
}

static void 
qstack_thread_start(qstack_t qstack)
{
#ifdef COROUTINE_SCHEDULING
    qCoAttr_t attr;
    attr.stack_size = 8*1024 * 1024;
	
	// create new use-level stack thread and resume it
	q_coCreate(&qstack->thread, &attr, qstack_main_loop, qstack);
	TRACE_THREAD("stack thread created @ Core %d\n", qstack->stack_id);
	TRACE_THREAD("try to resume to stack thread @ Core %d!\n", 
			qstack->stack_id);
	qstack->rt_ctx->on_stack = 1;
	q_coResume(qstack->thread);
#else
	qstack_main_loop(qstack);
#endif
}
/******************************************************************************/
/* functions */
void 
qstack_init(int stack_num)
{
	int i;
	qstack_t qstack;
	init_system_ts();	// basic timestamp when system start
	fp_log = fopen(LOG_PATH, "w");
	fp_screen = fopen(SCREEN_PATH, "w");
	fmsgq_init();
	msgq_init(MAX_CORE_NUM);
	core_map_init(core_map);

	TRACE_LOG("========================================\n"
			"Qingyun network stack start!\n"
			"========================================\n");
	test_func1();
//	TRACE_EXIT("mbuf size: %d\n", sizeof(struct rte_mbuf));
//	TRACE_EXIT("tcp_stream size: %d\n", sizeof(struct tcp_stream));

	// TODO: init the config
	CONFIG.max_concurrency = MAX_FLOW_NUM;
//	CONFIG.num_cores = MAX_CORE_NUM;
//	CONFIG.num_stacks = min(CONFIG.num_stacks, MAX_STACK_NUM);
//	CONFIG.num_stacks = MAX_STACK_NUM;
//	CONFIG.num_apps = 3;
//	CONFIG.num_servers = MAX_SERVER_NUM;
	CONFIG.core_offset = 0;
	CONFIG.rcvbuf_size = STATIC_BUFF_SIZE;
	CONFIG.sndbuf_size = STATIC_BUFF_SIZE;
	CONFIG.eths = (struct eth_table*)calloc(1, sizeof(struct eth_table));

	if (CONFIG.num_cores > MAX_CORE_NUM) {
		TRACE_ERROR("Wrong CONFIG.num_cores: %d, MAX_CORE_NUM: %d\n",
				CONFIG.num_cores, MAX_CORE_NUM);
	}         
	if (CONFIG.num_stacks > MAX_STACK_NUM) {
		TRACE_ERROR("Wrong CONFIG.num_stacks: %d, MAX_STACK_NUM: %d\n",
				CONFIG.num_stacks, MAX_STACK_NUM);
	}         
	if (CONFIG.num_servers > MAX_SERVER_NUM) {
		TRACE_ERROR("Wrong CONFIG.num_servers: %d, MAX_SERVER_NUM: %d\n",
				CONFIG.num_servers, MAX_SERVER_NUM);
	}


	CONFIG.eths_num = 1;
	CONFIG.eths[0].ifindex = 0;

	TRACE_LOG("start io_module initialization\n");
	io_init();
	test_func2();

	TRACE_LOG("start global context initialization\n");
	gctx_init();
	//io_get_rxtx_ten(NULL); // polling check core start
	srand(time(NULL));

    pthread_attr_t attr;
    cpu_set_t cpus;
	for (i=0; i<stack_num; i++) {
		qstack = stack_context_init(i);

		pthread_attr_init(&attr);
    	CPU_ZERO(&cpus);
		CPU_SET(core_map[i]+CONFIG.core_offset, &cpus);	// add CPU i into cpu set "cpus"
    	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);

    	pthread_create(&qstack->rt_ctx->rt_thread, &attr, qstack_thread_start, 
				(void *)qstack);
 	}
#ifdef MONITOR_THREAD_CORE
    pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(MONITOR_THREAD_CORE, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);

    pthread_create(&get_global_ctx()->monitor_thread, &attr,
	            monitor_thread_start, (void *)qstack);
#endif
#if USE_MESSAGE
    pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(MSG_THREAD_CORE, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    pthread_create(&get_global_ctx()->log_thread, &attr,
	            msg_thread_start, (void *)qstack);

    pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(LOG_THREAD_CORE, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    pthread_create(&get_global_ctx()->log_thread, &attr,
	            log_thread_start, (void *)qstack);
#endif

	TRACE_CHECKP("qstack system global_init finish!\n");

	q_init_manager(stack_num, CONFIG.num_servers);
}  

int
qstack_create_app(int core_id, qapp_t *app_handle, app_func_t app_func, 
		void *args)
{
	qapp_t ret =  __qstack_create_app(core_id, app_func, args);
	if (unlikely(!ret)) {
		TRACE_ERR("failed to alloc apoplication thread!\n");
		return FALSE;
	} else {
		if (app_handle != NULL) {
			// when app_handle is NULL, the user don't need a qapp as return
			*app_handle = ret;
		}
		return SUCCESS;
	}
}

void
qstack_join()
{
	int i;
	pthread_t thread;
	for (i=0; i<CONFIG.num_cores; i++) {
		thread = get_core_context(i)->rt_ctx->rt_thread;
		if (thread != NULL) {
			pthread_join(thread, NULL);
		}
	}
}
/******************************************************************************/
