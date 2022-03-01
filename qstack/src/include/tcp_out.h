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
 * @file tcp_out.h
 * @brief output and generate tcp packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.8.22
 * @version 0.1
 * @detail Function list: \n
 *   1. enqueue_ack(): add stream to ack queue \n
 *   2. control_queue_add(): add stream to control queue \n
 *   3. send_event_add(): add stream to send event queue \n
 *   4. send_queue_add(): add stream to send queue \n
 *   5. write_tcp_control_queue(): process streams in the control queue, 
 *   	generate and send out control packets \n
 *   6. write_tcp_send_queue(): process streams in the send queue, generate and 
 *   	send out data packets \n
 *   7. write_tcp_ack_queue(): process streams in the ack queue, generate and 
 *   	send out ack packets \n
 *   8. send_tcp_packet_standalone(): generate and send tcp packet without 
 *   	stream context \n
 *   10. send_hevent_add(): add high-priority send event to target stack \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.18
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
#ifndef __TCP_OUT_H_
#define __TCP_OUT_H_
/******************************************************************************/
#include "protocol.h"
#include "qstack.h"
/******************************************************************************/
/* global macros */
/*----------------------------------------------------------------------------*/
#define SEND_WINDOW_MIN			1500
/******************************************************************************/
/* data structures */
enum ack_opt
{
	ACK_OPT_NOW, 		///< exceptions, send ack right now
	ACK_OPT_AGGREGATE, 	///< acks for received data
	ACK_OPT_WACK		///< ?
};
/******************************************************************************/
/* function declarations */
/**
 * add stream to ack_queue and wait to be sent out
 * 
 * @param 	qstack stack process context
 * @param 	cur_stream target stream
 * @param 	opt ack types \n
 * 				- ACK_OPT_NOW \n
 * 				- ACK_OPT_AGGREGATE \n
 * 				- ACK_OPT_WACK
 * 
 * @return
 * 	return SUCCESS if add to ack_queue or the steram is already in the 
 * 	ack_queue; 
 * 	return ERROR if the queue is full and failed to add.
 */
int
enqueue_ack(qstack_t qstack, tcp_stream *cur_stream, uint8_t opt);

/**
 * add stream to control_queue and wait to be sent out
 * 
 * @param qstack 		stack process context
 * @param cur_stream 	target stream
 * 
 * @return
 * 	return SUCCESS if add to the queue or the steram is already in the queue; 
 * 	return ERROR if the queue is full and failed to add.
 */
int 
control_queue_add(qstack_t qstack, tcp_stream_t cur_stream);

/**
 * add stream into send_queue
 *
 * @param qstack		stack process context
 * @param cur_stream	target stream
 *
 * @return
 * 	return SUCCESS if add to the queue or it's already on the send_queue; 
 * 	return ERROR if the queue is full and failed to add.
 *
 * @note
 * 	only called when flushing user events
 */
int
send_queue_add(qstack_t qstack, tcp_stream_t cur_stream);
/*----------------------------------------------------------------------------*/
/**
 * process tcp streams in the control_queue and send out control packets
 *
 * @param qstack 	stack process context
 * @param cur_ts 	timestamp for tcp header
 *
 * @return 
 * 	return the num of packets sent out
 */
int 
write_tcp_control_queue(qstack_t qstack, uint32_t cur_ts);

/**
 * process tcp streams in the send_queue and send out packets with payloaad
 *
 * @param qstack 	stack process context
 * @param cur_ts 	timestamp for tcp header
 *
 * @return 
 * 	return the num of packets sent out
 */
int 
write_tcp_send_queue(qstack_t qstack, uint32_t cur_ts);

/**
 * process tcp streams in the ack_queue and send out ack packets
 *
 * @param qstack 	stack process context
 * @param cur_ts 	timestamp for tcp header
 *
 * @return 
 * 	return the num of packets sent out
 */
int 
write_tcp_ack_queue(qstack_t qstack, uint32_t cur_ts);
/*----------------------------------------------------------------------------*/
/**
 * Send tcp packet without flow context. This functions is always called to 
 * send control packets like SYN or SYNACK
 *
 * @param qstack 		stack process context
 * @param saddr 		source ip address
 * @param sport 		source tcp port
 * @param daddr 		dest ip address
 * @param dport 		dest tcp port
 * @param seq 			sequence number in tcp header
 * @param ack_seq 		ack sequence number in tcp header
 * @param window 		window size in tcp header
 * @param flags 		tcp flags in tcp header
 * @param cur_ts 		timestamp for tcp header
 * @param echo_ts 		echo_ts in tcp header
 *
 * @return 
 * 	return SUCCESS if send out packet;
 * 	return FAILED if failed to send (e.g. the send queue is full)
 * 	return ERROR if error (e.g. stream not exist)
 */
int
send_tcp_packet_standalone(qstack_t qstack, uint32_t saddr, uint16_t sport, 
		uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack_seq, 
		uint16_t window, uint8_t flags, uint32_t cur_ts, uint32_t echo_ts);

/**
 * send an mbuf to TCP snd_buff
 *
 * @param core_id 	current core_id
 * @param sockid 	socket id
 * @param mbuf 		target mbuf to be sent
 * @param len 		tcp payload length writen in the mbuf
 * @param flags 	control flags:
 * 						QFLAG_SEND_HIGHPRI: high priority packet
 *
 * @return
 * 	return size of bytes writen to snd_buff, which is always equal to the param 
 * 	"len", and return FAILED(0) if failed to add mbuf to snd_buff, or ERROR(-1) 
 * 	if received unexpected parameters.
 * @note
 * 	the mbuf to be sent should be allocated by calling q_get_wmbuf()
 */
int
_q_tcp_send(int core_id, int sockid, mbuf_t mbuf, uint32_t len, uint8_t flags);
/******************************************************************************/
/**
 * select a q_id from the given core_id for send evnet
 *
 * @param core_id	the core_id of the core where is send event come from
 *
 * @return
 * 	the target q_id
 */
static inline uint8_t
send_event_queue_select(uint8_t core_id)
{
#if 0
	if (core_id < CONFIG.num_stacks) {
		// it's from the host stack core
		return CONFIG.num_stacks;
	} else {
		// it's from the server core
		return core_id - CONFIG.num_stacks;
	}
#endif 
	return core_id;
}

static inline int
send_event_enqueue(stream_queue_t q, uint8_t core_id, tcp_stream_t cur_stream)
{
	uint8_t q_id = send_event_queue_select(core_id);
	TRACE_SENDQ("add to %d send queue %p @ Stream %d, head %d, size %d\n", 
			q_id, &q->queues[q_id].queue[q->queues[q_id].head], cur_stream->id, 
			q->queues[q_id].head, q->queues[q_id].size);
	return streamq_enqueue(q, q_id, cur_stream);
}

/******************************************************************************/
/* inline functions */
/**
 * add stream to send_event_queue and wait to be flushed to send_queue
 * 
 * @param qstack 		stack process context
 * @param core_id		id of the core where the event comes from
 * @param cur_stream 	target stream
 * @param pri			priority
 * 
 * @return
 * 	return SUCCESS if add to the queue; 
 * 	return ERROR if the queue is full and failed to add.
 *
 * @note
 * 	usually called by application thread
 */
static inline int
send_event_add(qstack_t qstack, int core_id, tcp_stream_t cur_stream, int pri)
{
	// qapp->app_id is better
	stream_queue_t q;
	if (pri) {
		q = &qstack->send_hevent_queue;
	} else {
		q = &qstack->send_event_queue;
	}
	return send_event_enqueue(q, core_id, cur_stream);
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef __TCP_OUT_H_
