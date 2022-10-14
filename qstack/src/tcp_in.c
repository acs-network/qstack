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
 * @file tcp_in.c
 * @brief process coming tcp packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.19
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.9
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DETAIL
#endif
/*----------------------------------------------------------------------------*/
#include "universal.h"
#include "tcp_in.h"
#include "tcp_out.h"
#include "tcp_send_buff.h"
#include "api.h"
#include "timer.h"
/******************************************************************************/
/* local macros */
#define VERIFY_RX_CHECKSUM TRUE
#define RECOVERY_AFTER_LOSS TRUE
#define SELECTIVE_WRITE_EVENT_NOTIFY TRUE
/******************************************************************************/
/* forward declarations */
static int 
process_tcp_payload(qstack_t qstack, tcp_stream_t cur_stream, 
		uint32_t cur_ts, mbuf_t mbuf, uint32_t seq, int payloadlen);
static void
process_tcp_ack(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, 
		uint16_t window, int payloadlen);
/******************************************************************************/
/* local static functions */
static inline void
virtual_server_process(qstack_t qstack, tcp_stream_t cur_stream)
{
#if DISABLE_SERVER
	#ifdef VIRTUAL_SERVER
	int sockid = cur_stream->id;
	struct qapp_context qapp;
	qapp.app_id = 0;
	qapp.core_id = qstack->stack_id+1;
	char *buf;
	char *response;
	uint32_t len,sndlen; 

	mbuf_t mbuf = q_recv(&qapp, sockid, &buf, &len, 0);
	mbuf_t send_mbuf = q_get_wmbuf(&qapp, &response, &sndlen);
	response[6] = 0x3;
	rs_ts_pass(mbuf, send_mbuf);
	q_write(&qapp, sockid, send_mbuf, len, 0);
	#else
	mbuf_t mbuf = rb_get(qstack->stack_id, cur_stream);
	#endif
	q_free_mbuf(qstack->stack_id, mbuf);
#endif
}

static inline int
add_pending_event(tcp_stream_t cur_stream, struct qepoll_event *event)
{
	cur_stream->pdev_st = pdev_st_WRITING;
	event_list_append(&cur_stream->pending_events, event);
	_mm_sfence();
	cur_stream->pdev_st = pdev_st_WRITEN;
	TRACE_EPOLL("pending event @ Stream %d!\n", cur_stream->id);
	if (cur_stream->socket->qe) {
		if (cur_stream->pdev_st != pdev_st_WRITEN ) {
			// the pending event has been read
			return SUCCESS;
		} else {
			TRACE_EPOLL("miss pending event @ Stream %d\n", cur_stream->id);
			if (cur_stream->pdev_st != pdev_st_WRITEN ) {
				// the pending event has been read
				TRACE_EPOLL("pending event surprisingly received @ Stream %d\n", 
						cur_stream->id);
				return SUCCESS;
			} else {
				// the pending event remain in the list
				event_list_pop(&cur_stream->pending_events);
				return FALSE;
			}
		}
	} else {
		return SUCCESS;
	}
}

static inline int
check_stream_event_validation(tcp_stream_t cur_stream, 
		struct qepoll_event *event)
{
	if (cur_stream->socket && cur_stream->socket->qe) {
		return 1;
	} else {
		int ret = add_pending_event(cur_stream, event);
		if (ret == SUCCESS) {
			// sucessfully add event to pending event list
			return 0;
		} else if (ret == 0) {
			// failed to add pending event, maybe the epoll is available
			return 1;
		}
	}
}
/*----------------------------------------------------------------------------*/
// event functions
static inline int
event_dest(int stack_id, tcp_stream_t cur_stream, int pri)
{
//	return cur_stream->id % CONFIG.num_servers;
	uint8_t ret = cur_stream->socket->default_app;
	if (ret == (uint8_t)-1) {
		if (CONFIG.num_stacks == CONFIG.num_servers) {
			ret = stack_id;
		} else {
			ret = cur_stream->id % CONFIG.num_servers;
		}
		cur_stream->socket->default_app = ret;
	}
	return ret;
}

static inline void
add_epoll_event(uint32_t event_type, int pri, qstack_t qstack, int sockid, 
		int dst, uint32_t cur_ts, tcp_stream_t cur_stream)
{
#if !DISABLE_SERVER
	struct qepoll_event *event;
	int core = qstack->stack_id;

	if (-1 != pri) {
		event = CreateQevent(pri, core, sockid, dst, Q_READ);
		TRACE_EVENT("add read/write event at core %d to %d with sockid: %d\n", 
				core, dst, sockid);
	} else {
		event = CreateQevent(1, core, sockid, dst, Q_ACCEPT);
		TRACE_EVENT("add accept event at core %d to %d with sockid: %d\n", 
				core, dst, sockid);
	}
	if (!event) {
		TRACE_ERR("failed to alloc event instance\n");
	}
#ifdef QVDEB
	event->flow_st = cur_stream->state;
#endif
	if (pri != -1 ) {
		if (!check_stream_event_validation(cur_stream, event)) 
			// the event has been added to the pending event list
			return;
	}
	int ret = AddTimeEvent(event_type, cur_ts, event);
	if (ret) {
		TRACE_EXCP("failed to add event from %d to %d\n", 
				qstack->stack_id, dst);
	} else {
		qapp_t dst_app = get_app_context(dst);
		if (dst_app->rt_ctx->qstack) {
			wakeup_app_thread(dst_app);
		}
	}
#endif
}

static inline void
_raise_read_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		int pri)
{
	if (cur_stream->socket) {
		if (pri) {
			DSTAT_ADD(qstack->read_hevent_num, 1);
		} else {
			DSTAT_ADD(qstack->read_event_num, 1);
		}
		add_epoll_event(Q_EPOLLIN, pri, qstack, cur_stream->id, 
				event_dest(qstack->stack_id, cur_stream, pri), 
				cur_ts, cur_stream);
	} else {
		TRACE_EXCP("try to raise read event without a socket!\n");
	}
}
	
static inline void
raise_write_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	if (cur_stream->socket) {
		add_epoll_event(Q_EPOLLOUT, 0, qstack, cur_stream->id, 
				event_dest(qstack->stack_id, cur_stream, 1), 
				cur_ts, cur_stream);
	} else {
		TRACE_EXCP("try to raise read event without a socket!\n");
	}
}
	
static inline void
raise_error_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	if (cur_stream->socket) {
		add_epoll_event(Q_EPOLLERR, 0, qstack, cur_stream->id, 
				event_dest(qstack->stack_id, cur_stream, 1), cur_ts, 
				cur_stream);
	} else {
		TRACE_EXCP("try to raise read event without a socket!\n");
	}
}
	
static inline void
raise_close_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	_raise_read_event(qstack, cur_stream, cur_ts, 1);
}
	
static inline void
_raise_accept_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	int ret;
	int accepter;
	struct tcp_listener *listener;
	listener = cur_stream->listener;
	accepter = event_dest(qstack->stack_id, cur_stream, 1);
#if !DISABLE_SERVER
	ret = streamq_enqueue(&listener->acceptq[accepter], qstack->stack_id, 
			cur_stream);
	if (ret < 0) {
		TRACE_EXCP("Stream %d: Failed to enqueue to "
				"the listen backlog!\n", cur_stream->id);
		cur_stream->close_reason = TCP_NOT_ACCEPTED;
		cur_stream->state = TCP_ST_CLOSED;
//		AddtoControlList(mtcp, cur_stream, cur_ts);
	}
	TRACE_CNCT("Stream %d inserted into acceptq %d.\n", 
			cur_stream->id, accepter);
#endif
	// TODO: timer, epoll
	/* raise an event to the listening socket */
	if (listener->socket && (listener->socket->epoll & Q_EPOLLIN)) {
		DSTAT_ADD(qstack->accept_event_num, 1);
		add_epoll_event(Q_EPOLLIN, -1, qstack, 
				listener->sockid, accepter, cur_ts, cur_stream);
	} else {
		TRACE_EXCP("failed to add EPOLLIN into listener socket!\n");
		if (!listener->socket) {
			TRACE_EXCP("empty socket of listener!\n");
		} else {
			TRACE_EXCP("listen socket not listening EPOLLIN events!\n");
		}
	}
}

static inline void
RaiseErrorEvent(qstack_t qstack, tcp_stream_t cur_stream)
{
	TRACE_TODO();
}
/*----------------------------------------------------------------------------*/
static inline struct tcp_listener *
syn_packet_filter(qstack_t qstack, uint32_t ip, uint16_t port)
{
	// TODO: fileter syn pakcet according to listeners
	struct tcp_listener *ret = get_global_ctx()->listeners->table[0];
//	ret = (struct tcp_listener *)ListenerHTSearch(
//			get_global_ctx()->listeners, &tcph->dest);
	
	if (ip != CONFIG.eths[0].ip_addr) {
		ret = NULL;
		TRACE_EXCP("SYN packet with wrong IP address!\n");
	}
	if (ntohs(port) != ret->port) {
		ret = NULL;
		TRACE_EXCP("SYN packet with wrong TCP port %d. Expected:%d\n", 
				ntohs(port), ret->port);
	}
	return ret;
}

static inline tcp_stream *
handle_passive_open(qstack_t qstack, uint32_t cur_ts, const struct iphdr *iph, 
		const struct tcphdr *tcph, uint32_t seq, uint16_t window)
{
	tcp_stream_t cur_stream = NULL;

	/* create new stream and add to flow hash table */
	socket_t socket = socket_alloc(qstack->stack_id, SOCK_TYPE_STREAM);
	socket->default_app = -1;
	cur_stream = tcp_stream_create(qstack, socket, 0, iph->daddr, tcph->dest, 
			iph->saddr, tcph->source, seq);
	if (!cur_stream) {
		TRACE_EXCP("Could not allocate tcp_stream!\n");
		return FALSE;
	}
	timeout_list_add(qstack, cur_stream, cur_ts);
	cur_stream->sndvar.peer_wnd = window;
	cur_stream->sndvar.cwnd = 10;
	ParseTCPOptions(cur_stream, cur_ts, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);

	return cur_stream;
}

static inline void
handle_active_open(qstack_t qstack, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, uint16_t window)
{
	cur_stream->rcvvar.irs = seq;
	cur_stream->snd_nxt = ack_seq;
	cur_stream->sndvar.peer_wnd = window;
	cur_stream->rcvvar.snd_wl1 = cur_stream->rcvvar.irs - 1;
	cur_stream->rcv_nxt = cur_stream->rcvvar.irs + 1;
	rb_update_seq(&cur_stream->rcvvar.rcvbuf, cur_stream->rcv_nxt);
	cur_stream->rcvvar.last_ack_seq = ack_seq;
	ParseTCPOptions(cur_stream, cur_ts, (uint8_t *)tcph + TCP_HEADER_LEN, 
			(tcph->doff << 2) - TCP_HEADER_LEN);
	cur_stream->sndvar.cwnd = ((cur_stream->sndvar.cwnd == 1)? 
			(cur_stream->sndvar.mss * 2): cur_stream->sndvar.mss);
	cur_stream->sndvar.ssthresh = cur_stream->sndvar.mss * 10;
	cur_stream->sndvar.nrtx = 0;
	rto_list_remove(qstack, cur_stream);
	cur_stream->state = TCP_ST_ESTABLISHED;
	TRACE_STATE("SYN_SENT to ESTABLISHED @ Stream\n", cur_stream->id);
}

/* - Function: check_sequence_validation
 * - Description: check if the tcp sequence is validate
 * - Parameters:
 *   1. 
 *   2. 
 * - Return:
 *		return TRUE if acceptablel otherwise return FAILED
 * - Others:
 * */
static inline int
check_sequence_validation(qstack_t qstack, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, int payloadlen)
{
	/* Protect Against Wrapped Sequence number (PAWS) */
	/* TCP sequence validation */
	if (!TCP_SEQ_BETWEEN(seq + payloadlen, cur_stream->rcv_nxt, 
				cur_stream->rcv_nxt + cur_stream->rcvvar.rcv_wnd)) {

		/* if RST bit is set, ignore the segment */
		if (tcph->rst) {
			TRACE_TRACE("RST packet @ Stream!\n", cur_stream->id);
			return FAILED;
		}
		if (cur_stream->state == TCP_ST_ESTABLISHED) {
			/* check if it is to get window advertisement */
			if (seq + 1 == cur_stream->rcv_nxt) {
				enqueue_ack(qstack, cur_stream, ACK_OPT_AGGREGATE);
				return FAILED;
			}

			if (TCP_SEQ_LEQ(seq, cur_stream->rcv_nxt)) {
				enqueue_ack(qstack, cur_stream, ACK_OPT_AGGREGATE);
			} else {
				enqueue_ack(qstack, cur_stream, ACK_OPT_NOW);
			}
		} else {
			if (cur_stream->state == TCP_ST_TIME_WAIT) {
				//TODO:
				timewait_list_add(qstack, cur_stream, cur_ts);
			}
			//TODO:
			control_queue_add(qstack, cur_stream);
		}
		return FAILED;
	}
	return TRUE;
}

static inline void 
NotifyConnectionReset(qstack_t qstack, tcp_stream *cur_stream)
{
	TRACE_TODO();
}

static inline int 
handle_rst(qstack_t qstack, tcp_stream *cur_stream, uint32_t ack_seq, 
		uint32_t cur_ts)
{
	/* TODO: we need reset validation logic */
	/* the sequence number of a RST should be inside window */
	/* (in SYN_SENT state, it should ack the previous SYN */
	if (cur_stream->state <= TCP_ST_SYN_SENT) {
		/* not handled here */
		return FALSE;
	}

	if (cur_stream->state == TCP_ST_SYN_RCVD) {
		if (ack_seq == cur_stream->snd_nxt) {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_RESET;
			tcp_stream_destroy(qstack, cur_stream);
		}
		return TRUE;
	}

	/* if the application is already closed the connection, 
	   just destroy the it */
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2 || 
			cur_stream->state == TCP_ST_LAST_ACK || 
			cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_TIME_WAIT) {
		cur_stream->state = TCP_ST_CLOSED;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		tcp_stream_destroy(qstack, cur_stream);
		return TRUE;
	}

	if (cur_stream->state >= TCP_ST_ESTABLISHED && 
			cur_stream->state <= TCP_ST_CLOSE_WAIT) {
		/* ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT */
		/* TODO: flush all the segment queues */
		//NotifyConnectionReset(qstack, cur_stream);
	}

	if (!(cur_stream->sndvar.on_closeq || cur_stream->sndvar.on_resetq)) {
		//cur_stream->state = TCP_ST_CLOSED;
		//tcp_stream_destroy(qstack, cur_stream);
		cur_stream->state = TCP_ST_CLOSE_WAIT;
		cur_stream->close_reason = TCP_RESET;
		//TODO
		raise_close_event(qstack, cur_stream, cur_ts);
	}

	return TRUE;
}

static inline tcp_stream *
CreateNewFlowHTEntry(qstack_t qstack, uint32_t cur_ts, const struct iphdr *iph, 
		int ip_len, const struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
		int payloadlen, uint16_t window)
{
	tcp_stream_t cur_stream;
	struct tcp_listener *listener;
	
	TRACE_CHECKP("try to create new tcp stream!\n");
	if (tcph->syn && !tcph->ack) {
		/* handle the SYN */
		listener = syn_packet_filter(qstack, iph->daddr, tcph->dest);
		if (unlikely(listener == NULL)) {
			TRACE_EXCP("Refusing SYN packet.\n");
			send_tcp_packet_standalone(qstack, iph->daddr, tcph->dest, 
					iph->saddr, tcph->source, 0, seq + payloadlen + 1, 0, 
					TCP_FLAG_RST | TCP_FLAG_ACK, cur_ts, 0);
			return NULL;
		}

		/* now accept the connection */
		cur_stream = handle_passive_open(qstack, cur_ts, iph, tcph, seq, window);
		if (!cur_stream) {
			TRACE_EXCP("Not available space in flow pool.\n");
			send_tcp_packet_standalone(qstack, iph->daddr, tcph->dest, 
					iph->saddr, tcph->source, 0, seq + payloadlen + 1, 0, 
					TCP_FLAG_RST | TCP_FLAG_ACK, cur_ts, 0);
			return NULL;
		}
		cur_stream->listener = listener;

		return cur_stream;
	} else if (tcph->rst) {
		TRACE_EXCP("Reset packet comes\n");
		/* for the reset packet, just discard */
		return NULL;
	} else {
		TRACE_EXCP("Weird packet comes. seq: %u ack: %u\n", seq, ack_seq);
		/* TODO: for else, discard and send a RST */
		/* if the ACK bit is off, respond with seq 0: 
		   <SEQ=0><ACK=SEG.SEQ+SEG.LEN><CTL=RST,ACK>
		   else (ACK bit is on):
		   <SEQ=SEG.ACK><CTL=RST>
		   */
		if (tcph->ack) {
			send_tcp_packet_standalone(qstack, iph->daddr, tcph->dest, 
					iph->saddr, tcph->source, ack_seq, 0, 0, TCP_FLAG_RST, 
					cur_ts, 0);
		} else {
			send_tcp_packet_standalone(qstack, iph->daddr, tcph->dest, 
					iph->saddr, tcph->source, 0, seq + payloadlen, 0, 
					TCP_FLAG_RST | TCP_FLAG_ACK, cur_ts, 0);
		}
		return NULL;
	}
}
/*----------------------------------------------------------------------------*/
static inline void 
Handle_TCP_ST_LISTEN (qstack_t qstack, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph)
{
	if (tcph->syn) {
		if (cur_stream->state == TCP_ST_LISTEN)
			cur_stream->rcv_nxt++;
		cur_stream->state = TCP_ST_SYN_RCVD;
		TRACE_STATE("LISTEN to SYN_RCVD @ Stream %d\n", cur_stream->id);
		TRACE_CNCT("state change to SYN_RCVD @ Stream %d\n", cur_stream->id);
		control_queue_add(qstack, cur_stream);
	} else {
		TRACE_EXCP("Stream %d (TCP_ST_LISTEN): Packet without SYN.\n", 
				cur_stream->id);
	}
}

static inline void 
Handle_TCP_ST_SYN_SENT (qstack_t qstack, uint32_t cur_ts, 
		tcp_stream* cur_stream, const struct iphdr* iph, struct tcphdr* tcph,
		uint32_t seq, uint32_t ack_seq, int payloadlen, uint16_t window)
{
	if (tcph->ack) {
		/* filter the unacceptable acks */
		if (TCP_SEQ_LEQ(ack_seq, cur_stream->sndvar.iss) || 
				TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
			TRACE_EXCP("invalid ack_seq at SYN_SENT @ Stream %d."
					"ack_seq:%lu iss:%lu, snd_nxt:%lu\n", 
					cur_stream->id, ack_seq, cur_stream->sndvar.iss, 
					cur_stream->snd_nxt);
			if (!tcph->rst) {
				send_tcp_packet_standalone(qstack, 
						iph->daddr, tcph->dest, iph->saddr, tcph->source, 
						ack_seq, 0, 0, TCP_FLAG_RST, cur_ts, 0);
			}
			return;
		}
		/* accept the ack */
		cur_stream->sndvar.snd_una++;
	}

	if (tcph->rst) {
		TRACE_EXCP("recieve RST at SYN_SENT @ Stream %d\n", cur_stream->id);
		if (tcph->ack) {
			cur_stream->state = TCP_ST_CLOSE_WAIT;
			cur_stream->close_reason = TCP_RESET;
			if (cur_stream->socket) {
				raise_error_event(qstack, cur_stream, cur_ts);
			} else {
				tcp_stream_destroy(qstack, cur_stream);
			}
		}
		return;
	}

	if (tcph->syn) {
		if (tcph->ack) {
			/* init tcp_stream and change to ESTABLISHED */
			handle_active_open(qstack, cur_stream, cur_ts, tcph, seq, 
					ack_seq, window);
			
			if (cur_stream->socket) {
				raise_write_event(qstack, cur_stream, cur_ts);
			} else {
				TRACE_EXCP("no socket at ESTABLISHED @ Stream\n", cur_stream->id);
				send_tcp_packet_standalone(qstack, iph->daddr, tcph->dest, 
						iph->saddr, tcph->source, 0, seq + payloadlen + 1, 0, 
						TCP_FLAG_RST | TCP_FLAG_ACK, cur_ts, 0);
				cur_stream->close_reason = TCP_ACTIVE_CLOSE;
				tcp_stream_destroy(qstack, cur_stream);
				return;
			}
			control_queue_add(qstack, cur_stream);
			timeout_list_add(qstack, cur_stream, cur_ts);

		} else {
			cur_stream->state = TCP_ST_SYN_RCVD;
			TRACE_STATE("SYN_SENT to SYN_RCVD @ Stream\n", cur_stream->id);
			cur_stream->snd_nxt = cur_stream->sndvar.iss;
			control_queue_add(qstack, cur_stream);
		}
	}
}

static inline void 
Handle_TCP_ST_SYN_RCVD (qstack_t qstack, uint32_t cur_ts,
		tcp_stream_t cur_stream, struct tcphdr* tcph, uint32_t ack_seq) 
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	if (tcph->ack) {
		uint32_t prior_cwnd;
		/* check if ACK of SYN */
		if (ack_seq != sndvar->iss + 1) {
			TRACE_EXCP("Stream %d (TCP_ST_SYN_RCVD): "
					"weird ack_seq: %u, iss: %u\n", 
					cur_stream->id, ack_seq, sndvar->iss);
			return;
		}
		
		sndvar->snd_una++;
		cur_stream->snd_nxt = ack_seq;
		prior_cwnd = sndvar->cwnd;
		sndvar->cwnd = ((prior_cwnd == 1)? 
				(sndvar->mss * 2): sndvar->mss);
		
		sndvar->nrtx = 0;
		cur_stream->rcv_nxt = cur_stream->rcvvar.irs + 1;
		rto_list_remove(qstack, cur_stream);
		ml_ts_reset(&qstack->mloop_ts);

		TRACE_STATE("SYN_RCVD to ESTABLISHED @ Stream %d\n", cur_stream->id);
		TRACE_CNCT("state change to ESTABLISHED @ Stream %d\n", cur_stream->id);
		cur_stream->state = TCP_ST_ESTABLISHED;
		DSTAT_ADD(qstack->stream_estb_num, 1);

		/* update listening socket */
		// TODO: listener filter
		struct tcp_listener *listener = get_global_ctx()->listeners->table[0];
		{
			_raise_accept_event(qstack, cur_stream, cur_ts);
		}
	} else {
		TRACE_EXCP("Stream %d (TCP_ST_SYN_RCVD): No ACK.\n", 
				cur_stream->id);
		/* retransmit SYN/ACK */
		cur_stream->snd_nxt = sndvar->iss;
		control_queue_add(qstack, cur_stream);
	}
}

static inline void 
Handle_TCP_ST_CLOSE_WAIT (qstack_t qstack, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, 
		uint32_t ack_seq, int payloadlen, uint16_t window) 
{
	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_EXCP("Stream %d (TCP_ST_CLOSE_WAIT): "
				"weird seq: %u, expected: %u\n",
				cur_stream->id, seq, cur_stream->rcv_nxt);
		control_queue_add(qstack, cur_stream);
		return;
	}

	process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
			payloadlen);
}

static inline void 
Handle_TCP_ST_LAST_ACK (qstack_t qstack, uint32_t cur_ts, 
		const struct iphdr *iph, int ip_len, tcp_stream* cur_stream, 
		struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, int payloadlen, 
		uint16_t window) 
{

	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_EXCP("Stream %d (TCP_ST_LAST_ACK): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt);
		return;
	}

	if (tcph->ack) {
		process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
				payloadlen);

		if (!cur_stream->sndvar.is_fin_sent) {
			/* the case that FIN is not sent yet */
			/* this is not ack for FIN, ignore */
			TRACE_EXCP("Stream %d (TCP_ST_LAST_ACK): ""No FIN sent yet.\n", 
					cur_stream->id);
			return;
		}

		/* check if ACK of FIN */
		// fss should be set when the close request from app is processed by 
		// stack
		if (ack_seq == cur_stream->sndvar.fss + 1) {
			cur_stream->sndvar.snd_una++;
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_PASSIVE_CLOSE;
			TRACE_STATE("LAST_ACK to CLOSED @ Stream %d\n", cur_stream->id);
			tcp_stream_destroy(qstack, cur_stream);
		} else {
			TRACE_EXCP("Stream %d (TCP_ST_LAST_ACK): Not ACK of FIN. "
					"ack_seq: %u, expected: %u\n", 
					cur_stream->id, ack_seq, cur_stream->sndvar.fss + 1);
			//cur_stream->snd_nxt = cur_stream->sndvar.fss;
			control_queue_add(qstack, cur_stream);
		}
	} else {
		// excption branch, hardly enter
		TRACE_EXCP("Stream %d (TCP_ST_LAST_ACK): No ACK\n", cur_stream->id);
		//cur_stream->snd_nxt = cur_stream->sndvar.fss;
		control_queue_add(qstack, cur_stream);
	}
}

static inline void 
Handle_TCP_ST_FIN_WAIT_1 (qstack_t qstack, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, 
		uint32_t ack_seq, mbuf_t mbuf, int payloadlen, uint16_t window) 
{

	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		TRACE_EXCP("Stream %d (TCP_ST_LAST_ACK): "
				"weird seq: %u, expected: %u\n", 
				cur_stream->id, seq, cur_stream->rcv_nxt);
		control_queue_add(qstack, cur_stream);
		return;
	}

	if (tcph->ack) {
		process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
				payloadlen);

		if (cur_stream->sndvar.is_fin_sent && 
				ack_seq == cur_stream->sndvar.fss + 1) {
			cur_stream->sndvar.snd_una = ack_seq;
			if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
				TRACE_CLOSE("Stream %d: update snd_nxt to %u\n", 
						cur_stream->id, ack_seq);
				cur_stream->snd_nxt = ack_seq;
			}
			//cur_stream->sndvar.snd_una++;
			cur_stream->sndvar.nrtx = 0;
			rto_list_remove(qstack, cur_stream);
			cur_stream->state = TCP_ST_FIN_WAIT_2;
			TRACE_CLOSE("tcp state change to TCP_ST_FIN_WAIT_2 @ Stream %d\n", 
					cur_stream->id);
			TRACE_STATE("FIN_WAIT_1 to FIN_WAIT_2 @ Stream %d\n", 
					cur_stream->id);
		}

	} else {
		TRACE_EXCP("Stream %d: does not contain an ack!\n", 
				cur_stream->id);
		return;
	}

	if (payloadlen > 0) {
		if (process_tcp_payload(qstack, cur_stream, cur_ts, mbuf, seq, 
					payloadlen)) {
			/* if return is TRUE, send ACK */
			enqueue_ack(qstack, cur_stream, ACK_OPT_AGGREGATE);
		} else {
			enqueue_ack(qstack, cur_stream, ACK_OPT_NOW);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload 
		 * (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->rcv_nxt++;

			if (cur_stream->state == TCP_ST_FIN_WAIT_1) {
				cur_stream->state = TCP_ST_CLOSING;
				TRACE_CLOSE("tcp state change to TCP_ST_CLOSING @ Stream %d\n", 
						cur_stream->id);
				TRACE_STATE("FIN_WAIT_1 to CLOSING @ Stream %d\n", 
						cur_stream->id);
			} else if (cur_stream->state == TCP_ST_FIN_WAIT_2) {
				cur_stream->state = TCP_ST_TIME_WAIT;
				TRACE_CLOSE("tcp state change to TCP_ST_TIME_WAIT "
						"@ Stream %d\n", 
						cur_stream->id);
				TRACE_STATE("FIN_WAIT_1 to TIME_WAIT @ Stream %d\n", 
						cur_stream->id);
				timewait_list_add(qstack, cur_stream, cur_ts);
			}
			control_queue_add(qstack, cur_stream);
		}
	}
}

static inline void 
Handle_TCP_ST_FIN_WAIT_2 (qstack_t qstack, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, 
		uint32_t ack_seq, mbuf_t mbuf, int payloadlen, uint16_t window)
{
	if (tcph->ack) {
		process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
				payloadlen);
	} else {
		TRACE_EXCP("Stream %d: does not contain an ack!\n", cur_stream->id);
		return;
	}

	if (payloadlen > 0) {
		if (process_tcp_payload(qstack, cur_stream, cur_ts, mbuf, seq, 
					payloadlen)) {
			/* if return is TRUE, send ACK */
			enqueue_ack(qstack, cur_stream, ACK_OPT_AGGREGATE);
		} else {
			enqueue_ack(qstack, cur_stream, ACK_OPT_NOW);
		}
	}

	if (tcph->fin) {
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload 
		 * (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->state = TCP_ST_TIME_WAIT;
			cur_stream->rcv_nxt++;
			TRACE_STATE("FIN_WAIT_2 to TIME_WAIT @ Stream %d\n", 
					cur_stream->id);
			TRACE_CLOSE("tcp state change to TCP_ST_TIME_WAIT @ Stream %d\n", 
					cur_stream->id);

			timewait_list_add(qstack, cur_stream, cur_ts);
			control_queue_add(qstack, cur_stream);
		}
	}
}

static inline void
Handle_TCP_ST_CLOSING (qstack_t qstack, uint32_t cur_ts, 
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, 
		uint32_t ack_seq, int payloadlen, uint16_t window) 
{

	if (tcph->ack) {
		process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
				payloadlen);

		if (!cur_stream->sndvar.is_fin_sent) {
			TRACE_EXCP("Stream %d (TCP_ST_CLOSING): "
					"No FIN sent yet.\n", cur_stream->id);
			return;
		}

		// check if ACK of FIN
		if (ack_seq != cur_stream->sndvar.fss + 1) {
			/* if the packet is not the ACK of FIN, ignore */
			TRACE_EXCP("packet with unexcepted ack_seq at CLOSING state"
					" @ Stream %d\n", cur_stream->id);
			return;
		}
		
		cur_stream->sndvar.snd_una = ack_seq;
		cur_stream->snd_nxt = ack_seq;

		cur_stream->state = TCP_ST_TIME_WAIT;
		TRACE_STATE("CLOSING to TIME_WAIT @ Stream %d\n", cur_stream->id);
		TRACE_CLOSE("tcp state change to TIME_WAIT @ Stream %d\n", 
				cur_stream->id);
		
		timewait_list_add(qstack, cur_stream, cur_ts);

	} else {
		TRACE_EXCP("Stream %d (TCP_ST_CLOSING): Not ACK\n", cur_stream->id);
		return;
	}
}
/******************************************************************************/
/* local static functions */
static inline void 
Handle_TCP_ST_ESTABLISHED (qstack_t qstack, uint32_t cur_ts,
		tcp_stream* cur_stream, struct tcphdr* tcph, uint32_t seq, 
		uint32_t ack_seq, mbuf_t mbuf, int payloadlen, uint16_t window) 
{
	if (tcph->syn) {
		//TODO:
		control_queue_add(qstack, cur_stream);
		return;
	}

	if (payloadlen > 0) {
		if (0 < process_tcp_payload(qstack, cur_stream, cur_ts, mbuf, seq, 
					payloadlen)) {
			/* if return is TRUE, send ACK */
			enqueue_ack(qstack, cur_stream, ACK_OPT_AGGREGATE);
		} else {
			enqueue_ack(qstack, cur_stream, ACK_OPT_NOW);
			DSTAT_ADD(qstack->req_dropped, 1);
		}
	}
	
	if (tcph->ack) {
		process_tcp_ack(qstack, cur_stream, cur_ts, tcph, seq, ack_seq, window, 
				payloadlen);
	}

	if (tcph->fin) {
		TRACE_CLOSE("FIN received @ Stream %d!\n", cur_stream->id);
		/* process the FIN only if the sequence is valid */
		/* FIN packet is allowed to push payload 
		 * (should we check for PSH flag)? */
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			TRACE_CLOSE("raise read event for closing @ Stream %d\n", 
					cur_stream->id);
			_raise_read_event(qstack, cur_stream, cur_ts, 0);
			cur_stream->state = TCP_ST_CLOSE_WAIT;
			TRACE_STATE("ESTABLISHED to CLOSE_WAIT @ Stream %d\n", 
					cur_stream->id);
			cur_stream->rcv_nxt++;
			control_queue_add(qstack, cur_stream);
			/* notify FIN to application */
		} else {
			TRACE_EXCP("data missing when receving FIN @ Stream %d\n", 
					cur_stream->id);
			enqueue_ack(qstack, cur_stream, ACK_OPT_NOW);
			return;
		}
	}
}
/*----------------------------------------------------------------------------*/
static void
process_tcp_ack(qstack_t qstack, tcp_stream *cur_stream, uint32_t cur_ts, 
		struct tcphdr *tcph, uint32_t seq, uint32_t ack_seq, 
		uint16_t window, int payloadlen)
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	uint32_t cwindow, cwindow_prev;
	uint32_t rmlen;
	uint32_t snd_wnd_prev;
	uint32_t right_wnd_edge;
	uint8_t dup;
	int ret;
	int ctl;
	snd_buff_t snd_buf = &sndvar->sndbuf; // validity checked

	cwindow = window;
	if (!tcph->syn) {
		cwindow = cwindow << sndvar->wscale_peer;
	}
	right_wnd_edge = sndvar->peer_wnd + cur_stream->rcvvar.snd_wl2;

	/* If ack overs the sending buffer, return */
	if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2 ||
			cur_stream->state == TCP_ST_CLOSING || 
			cur_stream->state == TCP_ST_CLOSE_WAIT || 
			cur_stream->state == TCP_ST_LAST_ACK) {
		if (sndvar->is_fin_sent && ack_seq == sndvar->fss + 1) {
			ack_seq--;
		}
	}
	//TODO:ack too much data
	if (TCP_SEQ_GT(ack_seq, snd_buf->head_seq + sb_len(snd_buf))) {
		TRACE_EXCP("ack too much data @ Stream %d! ack_seq: %d\n", cur_stream->id, ack_seq);
		return;
	}
	/* Update window */
	if (TCP_SEQ_LT(cur_stream->rcvvar.snd_wl1, seq) ||
			(cur_stream->rcvvar.snd_wl1 == seq && 
			TCP_SEQ_LT(cur_stream->rcvvar.snd_wl2, ack_seq)) ||
			(cur_stream->rcvvar.snd_wl2 == ack_seq && 
			cwindow > sndvar->peer_wnd)) {
		cwindow_prev = sndvar->peer_wnd;
		sndvar->peer_wnd = cwindow;
		cur_stream->rcvvar.snd_wl1 = seq;
		cur_stream->rcvvar.snd_wl2 = ack_seq;
		if (cwindow_prev < cur_stream->snd_nxt - sndvar->snd_una && 
				sndvar->peer_wnd >= cur_stream->snd_nxt - sndvar->snd_una) {
			//TODO
//			TODO RAISE: raise_write_event(qstack, cur_stream, cur_ts);
		}
	}

	/* Check duplicated ack count */
	/* Duplicated ack if 
	   1) ack_seq is old
	   2) payload length is 0.
	   3) advertised window not changed.
	   4) there is outstanding unacknowledged data
	   5) ack_seq == snd_una
	 */

	dup = FALSE;
	if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
		if (ack_seq == cur_stream->rcvvar.last_ack_seq && payloadlen == 0) {
			if (cur_stream->rcvvar.snd_wl2 + sndvar->peer_wnd == 
					right_wnd_edge) {
				if (cur_stream->rcvvar.dup_acks + 1 > 
						cur_stream->rcvvar.dup_acks) {
					cur_stream->rcvvar.dup_acks++;
				}
				dup = TRUE;
			}
		}
	}
	if (!dup) {
		cur_stream->rcvvar.dup_acks = 0;
		cur_stream->rcvvar.last_ack_seq = ack_seq;
	}

	/* Fast retransmission */
	if (dup && cur_stream->rcvvar.dup_acks == 3) {
		if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
			cur_stream->snd_nxt = ack_seq;
			sb_reset_retrans(snd_buf);
		}

		/* update congestion control variables */
		/* ssthresh to half of min of cwnd and peer wnd */
		sndvar->ssthresh = MIN(sndvar->cwnd, sndvar->peer_wnd) / 2;
		if (sndvar->ssthresh < 2 * sndvar->mss) {
			sndvar->ssthresh = 2 * sndvar->mss;
		}
		sndvar->cwnd = sndvar->ssthresh + 3 * sndvar->mss;

		/* count number of retransmissions */
		//TODO
		if (sndvar->nrtx < TCP_MAX_RTX) {
			sndvar->nrtx++;
		} else {
			//too much retransmissions
		}
		/* add to send queue */
		send_event_enqueue(&qstack->send_event_queue, qstack->stack_id, 
				cur_stream);

	} else if (cur_stream->rcvvar.dup_acks > 3) {
		/* Inflate congestion window until before overflow */
		if ((uint32_t)(sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
			sndvar->cwnd += sndvar->mss;
		}
	}

#if RECOVERY_AFTER_LOSS //(TRUE)
	/* updating snd_nxt (when recovered from loss) */
	if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
		cur_stream->snd_nxt = ack_seq;
		if (sb_len(snd_buf) == 0) {
			//TODO:
//			RemoveFromSendList(qstack, cur_stream);
		}
	}
#endif

	/* If ack_seq is previously acked, return */
	if (TCP_SEQ_GEQ(snd_buf->head_seq, ack_seq)) {
		return;
	}

	/* Remove acked sequence from send buffer */
	rmlen = ack_seq - snd_buf->head_seq;
	if (rmlen > 0) {
		/* Routine goes here only if there is new payload (not retransmitted) */
		uint16_t packets;

		/* If acks new data */
		packets = rmlen / sndvar->eff_mss;
		if ((rmlen / sndvar->eff_mss) * sndvar->eff_mss > rmlen) {
			packets++;
		}
		
		// TODO: rtt estimate
		/* Update congestion control variables */
		if (cur_stream->state >= TCP_ST_ESTABLISHED) {
			if (sndvar->cwnd < sndvar->ssthresh) {
				if ((sndvar->cwnd + sndvar->mss) > sndvar->cwnd) {
					sndvar->cwnd += (sndvar->mss * packets);
				}
			} else {
				uint32_t new_cwnd = sndvar->cwnd + 
						packets * sndvar->mss * sndvar->mss / 
						sndvar->cwnd;
				if (new_cwnd > sndvar->cwnd) {
					sndvar->cwnd = new_cwnd;
				}
			}
		}

		ret = sb_remove_acked(qstack, snd_buf, ack_seq);
		sndvar->snd_una = ack_seq;
		snd_wnd_prev = sndvar->snd_wnd;
		uint32_t sb_length = sb_len(snd_buf);
		sndvar->snd_wnd = snd_buf->size - sb_length;
		if (cur_stream->snd_nxt == ack_seq) { 
			//uack queue is empty
			rto_list_remove(qstack, cur_stream);
		} else { 
			// there are still unacked packets
			rto_list_update(qstack, cur_stream, cur_ts);
		}

		/* If there was no available sending window */
		/* notify the newly available window to application */
#if SELECTIVE_WRITE_EVENT_NOTIFY //(TRUE)
		if (snd_wnd_prev <= 0) {
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */
		// TODO RAISE: raise_write_event(qstack, cur_stream, cur_ts);
#if SELECTIVE_WRITE_EVENT_NOTIFY
		}
#endif /* SELECTIVE_WRITE_EVENT_NOTIFY */
	}
}

static inline void
check_mbuf_priority(qstack_t qstack, tcp_stream_t cur_stream, mbuf_t mbuf)
{
#if USER_DEFINED_PRIORITY
	#ifdef UNIFORM_PRIORITY
	mbuf->priority = UNIFORM_PRIORITY;
	#else
	char *payload;
	
	if (cur_stream->if_req_high == NULL) {
		// if not supported, just return.
		return;
	}
	payload = mbuf_get_payload_ptr(mbuf); 

	if (cur_stream->if_req_head == NULL) {
		// default condition, check the payload of  every packet
		mbuf->priority = cur_stream->if_req_high(payload);
	} else {
		if (cur_stream->if_req_head(payload)) {
			// it's the head of a new message, check it
			mbuf->priority = cur_stream->if_req_high(payload);
			if (mbuf->priority) {
				cur_stream->next_high_pri_msg_seq = 
						mbuf->tcp_seq + mbuf->payload_len;
			}
		} else {
			// check whether it's the following part of a high-pri message
			if (mbuf->tcp_seq == cur_stream->next_high_pri_msg_seq) {
				mbuf->priority = 1;
				cur_stream->next_high_pri_msg_seq = 
						mbuf->tcp_seq + mbuf->payload_len;
			}
		}
	}
	#endif
	if (mbuf->priority) {
		BSTAT_ADD(qstack->hreq_recv_num, 1);
	} else {
		BSTAT_ADD(qstack->lreq_recv_num, 1);
	}
#endif
}

/**
 * process tcp payload
 *
 * @param qstack stack process context
 * @param cur_stream target stream
 * @param cur_ts timestamp	//is necessary?
 * @param mbuf target mbuf
 * @param seq tcp sequence
 * @param payloadlen length of tcp payload
 *
 * @return
 * 	return SUCCESS if successfully process payload and put in receive buffer;
 * 	otherwise return FAILED or ERROR;
 */
static int 
process_tcp_payload(qstack_t qstack, tcp_stream_t cur_stream, 
		uint32_t cur_ts, mbuf_t mbuf, uint32_t seq, int payloadlen)
{
	struct tcp_recv_vars *rcvvar = &cur_stream->rcvvar;
	uint32_t prev_rcv_nxt;
	int ret, i;
	static int drop_count = 0;
	TRACE_CHECKP("process tcp payload @ Stream %d\n", cur_stream->id);
//	mbuf_print_info(mbuf);
//	q_prefetch0(rcvvar->rcvbuf);

	/* if seq and segment length is lower than rcv_nxt, ignore and send ack */
	if (unlikely(TCP_SEQ_LEQ(seq + payloadlen, cur_stream->rcv_nxt))) {
		DSTAT_ADD(qstack->retrans_rcv_num, 1);
		TRACE_OOO("request drop! ip %u to %u, port %u to %u\n",
			htonl(cur_stream->daddr)&0xff, 
			htonl(cur_stream->saddr)&0xff, 
			htons(cur_stream->dport), 
			htons(cur_stream->sport));
		TRACE_OOO("unexcepted retransmit packet received @ Stream %d!"
				"seq: %u, payloadlen: %u, rcv_nxt: %u, "
				"last_req_ts:%llu, last_ack_ts:%llu\n"
				, cur_stream->id, seq, payloadlen, cur_stream->rcv_nxt
				, cur_stream->last_req_ts, cur_stream->last_ack_ts);
		return FAILED;
	}
	/* if payload exceeds receiving buffer, drop and send ack */
	if (unlikely(TCP_SEQ_GT(seq + payloadlen, 
					cur_stream->rcv_nxt + rcvvar->rcv_wnd))) {
		TRACE_EXCP("payload is out of recv bufferi @ Stream %d!\n"
				"seq: %u, payloadlen: %u, rcv_nxt: %u\n"
				, cur_stream->id
				, cur_stream->id, seq, payloadlen, cur_stream->rcv_nxt);
		return FAILED;
	}
	{
		/* process normal TCP payload */
		prev_rcv_nxt = cur_stream->rcv_nxt;
		check_mbuf_priority(qstack, cur_stream, mbuf);
		ret = rb_put(qstack, cur_stream, mbuf);
		if (unlikely(ret == FAILED)) {
			TRACE_OOO("failed to put mbuf %p into rcv_buff @ Stream %d!\n", 
					mbuf, cur_stream->id);
		} else {
			// do not excute virtual server process if failed to put the mbuf 
			// into receive buffer
			if (mbuf->priority) {
				BSTAT_ADD(qstack->hreq_recv_num, 1);
			} else {
				BSTAT_ADD(qstack->lreq_recv_num, 1);
			}
			virtual_server_process(qstack, cur_stream);
		}
	
		/* discard the buffer if the state is FIN_WAIT_1 or FIN_WAIT_2, 
		   meaning that the connection is already closed by the application */
		if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
				cur_stream->state == TCP_ST_FIN_WAIT_2) {
			rb_clear(qstack, &cur_stream->rcvvar.rcvbuf);
		}
	}
	cur_stream->rcv_nxt = rcvvar->rcvbuf.merged_next;
	rcvvar->rcv_wnd = rcvvar->rcvbuf.size - rb_merged_len(&rcvvar->rcvbuf);

#ifdef PRIORITY_RECV_BUFF
	/* do not trate it as out of order, may be a result of NIC priority */
	if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, prev_rcv_nxt) && !mbuf->priority) 
#else
	if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, prev_rcv_nxt)) 
#endif
	{
		/* rcv_nxt not updated, some packet may be lost */
		TRACE_OOO("some packets may be lost @ Stream %d\n"
				"rcv_nxt: %u, prev_rcv_nxt: %u, NIC drop: %u\n",
				cur_stream->id, cur_stream->rcv_nxt, prev_rcv_nxt, 
				io_get_rx_err(0));
		return FALSE;
	}

	if (cur_stream->state == TCP_ST_ESTABLISHED) {
		/* ret is the result of rb_put() or process_ssl_packet() */
		int event_num = ret;
#if PRIORITY_RECV
		if (mbuf->priority) {
			_raise_read_event(qstack, cur_stream, cur_ts, 1);
			event_num--;
		}
#endif
		if (event_num>0) {
				_raise_read_event(qstack, cur_stream, cur_ts, 0);
		}
	}
	return ret;
}
/******************************************************************************/
/* functions */
void
raise_read_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		int pri)
{
	_raise_read_event(qstack, cur_stream, cur_ts, pri);
}

void
raise_accept_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	_raise_accept_event(qstack, cur_stream, cur_ts);
}

int
process_tcp_packet(qstack_t qstack, uint32_t cur_ts, const int ifidx,  
		mbuf_t mbuf, int ip_len)
{
//	struct tcphdr* tcph = (struct tcphdr *) ((u_char *)iph + (iph->ihl << 2));
	struct ethhdr *ethh = (struct ethhdr *)mbuf_get_buff_ptr(mbuf);
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	uint8_t *payload    = (uint8_t *)tcph + (tcph->doff << 2);
	int payloadlen = ip_len - (payload - (uint8_t *)iph);
	tcp_stream_t cur_stream = NULL;
	struct ipv4_5tuple s_qtuple = {0};	// struct for hashing
	uint32_t seq = ntohl(tcph->seq);
	uint32_t ack_seq = ntohl(tcph->ack_seq);
	uint16_t window = ntohs(tcph->window);
	uint16_t check;
	int ret, i;
	int rc = -1;

	mbuf->tcp_seq = seq;
	mbuf->payload_len = payloadlen;
	mbuf->l4_len = payload - (uint8_t *)tcph;
#if TCP_PRIORITY
	if (payloadlen) {
	#ifndef UNIFORM_PRIORITY
		if (payload[5] == 0x01) 
	#else
		if (UNIFORM_PRIORITY) 
	#endif
		{
			mbuf->priority = 1;
		} else {
			mbuf->priority = 0;
	#ifdef RSTS_HP_ONLY
			rs_ts_clear(mbuf);
	#endif
		}
	}
#endif
	/* Check ip packet invalidation */	
	if (unlikely(!tcph->doff || ip_len < ((iph->ihl + tcph->doff) << 2))) {
		TRACE_EXCP("invalied ip packet %p!\n", mbuf);
		return ERROR;
	}
	// only use software tcp_checksum

	s_qtuple.proto = 6;				// TCP protocol
	s_qtuple.ip_src = iph->daddr;	// regard the locol host as source
	s_qtuple.ip_dst = iph->saddr;
	s_qtuple.port_src = tcph->dest;
	s_qtuple.port_dst = tcph->source;

	/* if the stream differed by quintuple is not found, then it's
	 * a new stream, creat new stream and add it to stream table */
	// TODO: dynamic qstack_id in multi-core mode
	if (!(cur_stream = StreamHTSearch(
					get_global_ctx()->stream_ht[qstack->stack_id], 
					&s_qtuple))) {
		/* not found in flow table */
		if (unlikely(!tcph->syn)) {
			TRACE_EXCP("packet %p from unknown stream! "
					"mbuf seq:%u ack_seq:%u payload_len:%u "
					"fin:%d, rst:%d "
//					"saddr: %s, daddr: %s, "
					"saddr: %d.%d, daddr: %d.%d"
					"sport: %u, dport: %u\n", 
					mbuf
					, seq, ack_seq, payloadlen
					, tcph->fin, tcph->rst
//					, inet_ntoa(iph->saddr), inet_ntoa(iph->daddr)
					, (iph->saddr & 0xff0000)>>16, iph->saddr >>24
					, (iph->daddr & 0xff0000)>>16, iph->daddr >>24
					, ntohs(tcph->source), ntohs(tcph->dest));
			mbuf_print_detail(mbuf);

			return FAILED;
		}
		TRACE_CHECKP("Create new flow!\n");
		cur_stream = CreateNewFlowHTEntry(qstack, cur_ts, iph, ip_len, tcph, 
				seq, ack_seq, payloadlen, window);
//		cur_stream = handle_passive_open(qstack, 0, iph, tcph, seq, window);
		if (unlikely(!cur_stream)) {
			TRACE_EXCP("failed to alloc a new stream!\n");
			return FAILED;
		}
		for (i = 0; i < ETH_ALEN; i++) {
			cur_stream->dhw_addr[i] = ethh->h_source[i];
		}
		ml_ts_reset(&qstack->mloop_ts);
	} else {
		if (unlikely(tcph->syn && cur_stream->state != TCP_ST_SYN_SENT)) {
			ParseTCPOptions(cur_stream, cur_ts, (uint8_t *)tcph+TCP_HEADER_LEN,
					(tcph->doff << 2) - TCP_HEADER_LEN);
			TRACE_INIT("SYN packet from an existing stream "
					"with state %d @ Stream %d\n",
					cur_stream->state, cur_stream->id);
		}
	}
	cur_stream->last_active_ts = cur_ts;


	TRACE_CHECKP("process tcp packet %p @ Stack %d @ Stream %d! "
			"mbuf seq:%u ack_seq:%u payload_len:%u priority:%d "
			"src_port:%d dst_port:%d\n" 
			, mbuf, qstack->stack_id, cur_stream->id
			, seq, ack_seq, payloadlen, mbuf->priority
			, ntohs(tcph->source), ntohs(tcph->dest));

	/* Validate sequence. if not valid, ignore the packet */
	if (cur_stream->state > TCP_ST_SYN_RCVD) {
		ret = check_sequence_validation(qstack, cur_stream, 
				cur_ts, tcph, seq, ack_seq, payloadlen);
		if (ret != TRUE) {
			// packet with wrong tcp sequence number
			TRACE_OOO("packet %p with unexcepted seqnum @ Stream %d! "
					"seq: %u, rcv_nxt: %u, ack_seq: %u, "
					"rcv_wnd: %d, payload_len: %d\n"
					"mbuf info: seq: %u, stream_id: %u\n"
					, mbuf, cur_stream->id 
					, seq, cur_stream->rcv_nxt, ack_seq
					, cur_stream->rcvvar.rcv_wnd, payloadlen
					, mbuf->tcp_seq, mbuf->stream_id);
			return FAILED;
		}
	}
#if MBUF_STREAMID
	mbuf->stream_id = cur_stream->id;
#endif

	/* Update receive window size */
	if (tcph->syn) {
		TRACE_CNCT("receive syn packet with seq %u!\n", seq);
		cur_stream->sndvar.peer_wnd = window;
	} else {
		cur_stream->sndvar.peer_wnd = 
				(uint32_t)window << cur_stream->sndvar.wscale_peer;
	}
				
	timeout_list_update(qstack, cur_stream, cur_ts);

	/* Process RST: process here only if state > TCP_ST_SYN_SENT */
	if (tcph->rst) {
		TRACE_CLOSE("RST received @ Stream %d, state %d\n", 
				cur_stream, cur_stream->state);
		cur_stream->have_reset = TRUE;
		if (cur_stream->state > TCP_ST_SYN_SENT) {
			if (handle_rst(qstack, cur_stream, ack_seq, cur_ts)) {
				return TRUE;
			}
		}
	}

	switch (cur_stream->state) {
	case TCP_ST_LISTEN:
		Handle_TCP_ST_LISTEN(qstack, cur_ts, cur_stream, tcph);
		break;

	case TCP_ST_SYN_SENT:
		Handle_TCP_ST_SYN_SENT(qstack, cur_ts, cur_stream, iph, tcph, 
				seq, ack_seq, payloadlen, window);
		break;

	case TCP_ST_SYN_RCVD:
		/* SYN retransmit implies our SYN/ACK was lost. Resend */
		if (tcph->syn && seq == cur_stream->rcvvar.irs)
			Handle_TCP_ST_LISTEN(qstack, cur_ts, cur_stream, tcph);
		else {
			Handle_TCP_ST_SYN_RCVD(qstack, cur_ts, cur_stream, tcph, ack_seq);
			if (payloadlen > 0 && cur_stream->state == TCP_ST_ESTABLISHED) {
				Handle_TCP_ST_ESTABLISHED(qstack, cur_ts, cur_stream, tcph,
							  seq, ack_seq, mbuf, payloadlen, window);
			}
		}
		break;

	case TCP_ST_ESTABLISHED:
		Handle_TCP_ST_ESTABLISHED(qstack, cur_ts, cur_stream, tcph, 
				seq, ack_seq, mbuf, payloadlen, window);
		break;

	case TCP_ST_CLOSE_WAIT:
		Handle_TCP_ST_CLOSE_WAIT(qstack, cur_ts, cur_stream, tcph, seq, ack_seq,
				payloadlen, window);
		break;

	case TCP_ST_LAST_ACK:
		Handle_TCP_ST_LAST_ACK(qstack, cur_ts, iph, ip_len, cur_stream, tcph, 
				seq, ack_seq, payloadlen, window);
		break;
	
	case TCP_ST_FIN_WAIT_1:
		Handle_TCP_ST_FIN_WAIT_1(qstack, cur_ts, cur_stream, tcph, seq, ack_seq,
				mbuf, payloadlen, window);
		break;

	case TCP_ST_FIN_WAIT_2:
		Handle_TCP_ST_FIN_WAIT_2(qstack, cur_ts, cur_stream, tcph, seq, ack_seq, 
				mbuf, payloadlen, window);
		break;

	case TCP_ST_CLOSING:
		Handle_TCP_ST_CLOSING(qstack, cur_ts, cur_stream, tcph, seq, ack_seq,
				payloadlen, window);
		break;

	case TCP_ST_TIME_WAIT:
		//TODO
		/* the only thing that can arrive in this state is a retransmission 
		   of the remote FIN. Acknowledge it, and restart the 2 MSL timeout */
		if (cur_stream->on_timewait_list) {
			TRACE_CLOSE("unexcepted packets received at TIME_WAIT "
					"@ Stream %d\n", cur_stream->id);
			timewait_list_remove(qstack, cur_stream);
			timewait_list_add(qstack, cur_stream, cur_ts);
		}
		control_queue_add(qstack, cur_stream);
		break;

	case TCP_ST_CLOSED:
		break;

	}

	return TRUE;
}
