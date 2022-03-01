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
 * @file tcp_recv_buff.c
 * @brief receive buffers for tcp stream
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.4
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 6.28
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif
/*----------------------------------------------------------------------------*/
#include "tcp_recv_buff.h"
#include "tcp_stream.h"
#include "qstack.h"
#include "dpdk_module.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
/*----------------------------------------------------------------------------*/
/* inline functions for mbuf process */
/*----------------------------------------------------------------------------*/
/**
 * try to insert the mbuf into the out-of-order list
 *
 * @param buff target rcv_buff
 * @param mbuf target mbuf
 *
 * @return 
 * 	return SUCCESS if success; ohterwise return FAILED (duplicate data segment)
 */
static inline int
ooo_insert(rcv_buff_t buff, mbuf_t mbuf)
{
	mbuf_list_t list = &buff->ooo_list;
	mbuf_t p, next;
	uint8_t ret = FAILED;

	if (!list->head) {
		list->head = mbuf;
		list->tail = mbuf;
		ret = SUCCESS;
	} else {
		p = list->tail;
		if (TCP_SEQ_LEQ(p->tcp_seq + p->payload_len, mbuf->tcp_seq)) {
			// insert at tail
			mbuf_list_append(list, mbuf);
			ret = SUCCESS;
		} else {
			p = list->head;
			if (TCP_SEQ_LEQ(mbuf->tcp_seq + mbuf->payload_len, p->tcp_seq)) {
				// insert at head
				mbuf_list_add_head(mbuf, list);
				ret = SUCCESS;
			}
			else {
				while (next = p->buf_next) {
					if (TCP_SEQ_LEQ(p->tcp_seq + p->payload_len, mbuf->tcp_seq) 
							&& TCP_SEQ_LEQ(mbuf->tcp_seq + mbuf->payload_len, 
							next->tcp_seq)) {
						mbuf_list_insert(list, p, mbuf);
						ret = SUCCESS;
						break;
					} else {
						p = next;
					}
				}
			}
		}
	}
	mbuf_list_print_info(list);
	return ret;
}

static inline uint32_t
try_merge_ooo(qstack_t qstack, rcv_buff_t buff)
{
	mbuf_list_t list = &buff->ooo_list;
	mbuf_queue_t q = &buff->merged_q;
	mbuf_t mbuf;
	uint32_t ret = 0;
	while (list->head && TCP_SEQ_LEQ(list->head->tcp_seq, buff->merged_next)) {
		mbuf = mbuf_list_pop(list);
		if (mbuf->tcp_seq != buff->merged_next) {
			// it a out-of-data packet
			mbuf_free(qstack->stack_id, mbuf);
			continue;	
		}

		if (cirq_add(q, mbuf)) {
#ifdef PRIORITY_RECV_BUFF
			if (mbuf->priority == 0) {
				// the high-pri read event has already been raised
				ret++;
			}
#else
			ret++;
#endif
			buff->merged_next += mbuf->payload_len;
			TRACE_OOO("mbuf merged! seq:%u, merged_next: %u\n",
					mbuf->tcp_seq, buff->merged_next);
		} else {
			TRACE_EXCP("full merged_q and filed to merge mbuf %p with seq %u "
					"from ofo queue!\n", mbuf, mbuf->tcp_seq);
			mbuf_free(qstack->stack_id, mbuf);
			break;
		}
	}
	return ret;
}
/******************************************************************************/
/* functions */
rcv_buff_t 
rb_init(qstack_t qstack, rcv_buff_t buff, uint32_t init_seq)
{
	if (unlikely(!buff)) {
		TRACE_EXCP("no rcv_buff!\n");
		return NULL;
	}
	
	buff->init_seq = init_seq;
	// TODO: here size of merged_q and buff should be configured by config file
	mbufq_init(&buff->merged_q, &qstack->mp_mbufq);
	buff->size = 1400 * CONFIG.rcvbuf_size;
	buff->head_seq = init_seq;
	buff->merged_next = init_seq;

	mbuf_list_init(&buff->ooo_list);
#ifdef PRIORITY_RECV_BUFF
	mbufq_init(&buff->high_q, &qstack->mp_mbufq);
#endif
	return buff;
}

uint8_t
rb_free(qstack_t qstack, rcv_buff_t buff)
{
	TRACE_TODO();
}

int 
rb_put(qstack_t qstack, tcp_stream_t stream, mbuf_t mbuf)
{
	rcv_buff_t buff = &stream->rcvvar.rcvbuf;
	uint32_t buf_seq_next = mbuf->tcp_seq + mbuf->payload_len;
	int ret = 0;
#if 0
	if (unlikely(mbuf->pool != get_dpc(qstack->stack_id)->rx_pktmbuf_pool)) {
		TRACE_ERROR("try to put an error mbuf into rcvbuf!\n");
	}
#endif

	if (unlikely(!buff)) {
		TRACE_EXCP("no rcv_buff @ Stream %d!\n", stream->id);
		return ERROR;
	}
	if (unlikely(TCP_SEQ_GT(buf_seq_next, buff->head_seq + buff->size))) { 
		// out of rcv_buff
		TRACE_EXCP("out of rcv_buff @ Stream %d!\n", stream->id);
		return FAILED;
	}

	rs_ts_add(mbuf->q_ts, REQ_ST_REQIN);
#ifdef PRIORITY_RECV_BUFF
	mbuf->holding = 0;
	if (mbuf->priority) {
		if (mbufq_enqueue(&buff->high_q, mbuf) == SUCCESS) {
			mbuf->holding = 1;
			mbuf->mbuf_state = MBUF_STATE_RBUFFED;
		}
	}
#endif
#if INSTACK_TLS
	if (stream->is_ssl == 1) {
		// the seq will be always mergeable
		if (mbufq_enqueue(&buff->merged_q, mbuf)) {
			ret = 1;
		} else {
			TRACE_EXCP("failed to add ssl mbuf %p with seq %u to rcvbuf: "
					"full merged_q @ Stream %d\n", 
					mbuf, mbuf->tcp_seq, stream->id);
			return FAILED;
		}
	} else 
#endif
	if (mbuf->tcp_seq == buff->merged_next) {
		if (mbufq_enqueue(&buff->merged_q, mbuf)) {
			ret = 1;
			buff->merged_next = buf_seq_next;
			ret += try_merge_ooo(qstack, buff);
		} else {
			TRACE_EXCP("failed to add mbuf %p with seq %u to rcvbuf: "
					"full merged_q @ Stream %d\n", 
					mbuf, mbuf->tcp_seq, stream->id);
			return FAILED;
		}
	} else {
		TRACE_OOO("out of order @ Stream %d! merged_next: %u, mbuf_seq:%u\n", 
				stream->id, buff->merged_next, mbuf->tcp_seq);
		if (SUCCESS != ooo_insert(buff, mbuf)) {
			TRACE_OOO("duplicate ooo packet @ Stream %d!\n", stream->id);
			return FAILED;
		}
	}
	mbuf_set_op(mbuf, MBUF_OP_RB_SPUT, qstack->stack_id);
#ifdef RCVBUF_DUP_CHECK
	stream->mbuf_last_rb_in = mbuf;
#endif

#ifdef PRIORITY_RECV_BUFF
	if (!mbuf->priority) {
		// the mbuf_state would be already set to MBUF_STATE_RBUFFED when 
		// inserted into the high-pri rcvbuf
		mbuf->mbuf_state = MBUF_STATE_RBUFFED;
	}
#else
	mbuf->mbuf_state = MBUF_STATE_RBUFFED;
#endif
	
	DSTAT_ADD(qstack->req_add_num, 1);
	rb_print_info(buff);
	return ret;
}

static inline mbuf_t
try_get_high_mbuf(uint8_t core_id, rcv_buff_t buff, uint8_t is_ssl)
{
#ifdef PRIORITY_RECV_BUFF
	mbuf_t mbuf= NULL;
	while (mbuf = mbufq_dequeue(&buff->high_q)) {
		if (mbuf->mbuf_state == MBUF_STATE_RBUFFED) {
			// the mbuf is not read from low_rcvbuf
			TRACE_BUFF("get an mbuf from high_priority queue\n");
			if (mbuf->tcp_seq == buff->head_seq) {
				// the mbuf from high-pri queue is also the head of 
				// merged_queue (the first mbuf ready to be received)
				if (mbufq_dequeue(&buff->merged_q)) {
#if INSTACK_TLS
					if (is_ssl) {
						buff->head_seq += get_cipher_len(mbuf, mbuf->payload_len);
					} else 
#endif
					{
						buff->head_seq += mbuf->payload_len;
					}
					// change the refcnt thus the mbuf can be freed by app
					mbuf->holding = 0;
				}
			}
			return mbuf;
		} else {
			// the mbuf has been processed, just free it
			mbuf_free(core_id, mbuf);
		}
	}
	return mbuf;
#endif
}

static inline mbuf_t
try_get_merged_mbuf(uint8_t core_id, rcv_buff_t buff, uint8_t is_ssl)
{
#ifdef PRIORITY_RECV_BUFF
	mbuf_t mbuf= NULL;
	while (mbuf = mbufq_dequeue(&buff->merged_q)) {
		// update the head_seq first, no matter whether the mbuf has 
		// been processed
#if INSTACK_TLS
		if (is_ssl) {
			buff->head_seq += mbuf_get_tcp_payload_len(mbuf);
		} else 
#endif
		{
			buff->head_seq += mbuf->payload_len;
		}
		
		if (mbuf->mbuf_state == MBUF_STATE_RBUFFED) {
			// the mbuf is not read from high_rcvbuf, return it 
			TRACE_BUFF("get an mbuf from merged queue\n");
			break;
		} else {
			// the mbuf has been processed, just free it
			TRACE_BUFF("The mbuf from merged queue has already been read\n");
			mbuf_free(core_id, mbuf);
			DSTAT_ADD(get_global_ctx()->highjump_num[core_id], 1);
		}
	}
	return mbuf;
#endif
}

mbuf_t 
rb_get(uint8_t core_id, tcp_stream_t stream)
{
	rcv_buff_t buff = &stream->rcvvar.rcvbuf;
	if (!buff) {
		TRACE_EXCP("no rcv_buff!\n");
		return NULL;
	}
	mbuf_t mbuf = NULL;
	
#ifdef PRIORITY_RECV_BUFF
	#if INSTACK_TLS
	mbuf = try_get_high_mbuf(core_id, buff, stream->is_ssl);
	if (!mbuf) {
		mbuf = try_get_merged_mbuf(core_id, buff, stream->is_ssl);
	}
	#else
	mbuf = try_get_high_mbuf(core_id, buff, 0);
	if (!mbuf) {
		mbuf = try_get_merged_mbuf(core_id, buff, 0);
	}
	#endif
#else
	mbuf = mbufq_dequeue(&buff->merged_q);
	if (mbuf) {
	#if INSTACK_TLS
		if (stream->is_ssl) {
			buff->head_seq += get_cipher_len(mbuf, mbuf->payload_len);
		} else 
	#endif
		{
			buff->head_seq += mbuf->payload_len;
		}
	}
#endif
	if (mbuf && NULL == mbuf_get_payload_ptr(mbuf)) {
		TRACE_ERROR("get wrong mbuf from rcvbuf!\n");
	}
	rb_print_info(buff);
	return mbuf;
}

void 
rb_clear(qstack_t qstack, rcv_buff_t buff)
{
	mbufq_clear(qstack->stack_id, &buff->merged_q, &qstack->mp_mbufq);
	mbuf_list_free(qstack->stack_id, &buff->ooo_list);
#ifdef PRIORITY_RECV_BUFF
	mbufq_clear(qstack->stack_id, &buff->high_q, &qstack->mp_mbufq);
#endif
#if 0
	mbuf_t mbuf;
	while (mbuf = mbufq_dequeue(&buff->merged_q)) {
		mbuf_free(qstack->stack_id, mbuf);
	}
	while (mbuf = mbuf_list_pop(&buff->ooo_list)) {
		mbuf_free(qstack->stack_id, mbuf);
	}
#endif
}
/******************************************************************************/
