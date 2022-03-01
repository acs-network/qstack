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
 * @file tcp_send_buff.c
 * @brief tcp_send_buff management
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.9.12
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DTAIL
#endif
/*----------------------------------------------------------------------------*/
#include "debug.h"
#include "tcp_send_buff.h"
#include "qstack.h"
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
snd_buff_t
sb_init(qstack_t qstack, snd_buff_t buff, uint32_t init_seq)
{
	mbuf_list_init(&buff->uack_list); 
	mbuf_list_init(&buff->retrans_list);
	mbufq_init(&buff->usnd_q, &qstack->mp_mbufq);
	mbufq_init(&buff->husnd_q, &qstack->mp_mbufq);
		
	buff->size = 1400 * CONFIG.sndbuf_size;
	buff->head_seq = init_seq;
	buff->next_seq = init_seq;
	
	return buff;
}

int
sb_free(qstack_t qstack, snd_buff_t buff)
{
	mbufq_clear(qstack->stack_id, &buff->usnd_q, &qstack->mp_mbufq);
	mbuf_list_free(qstack->stack_id, &buff->uack_list);
	mbuf_list_free(qstack->stack_id, &buff->retrans_list);
	return SUCCESS;
}

int
sb_put(qstack_t qstack, snd_buff_t buff, mbuf_t mbuf)
{
	uint32_t to_put;
	uint32_t len = mbuf->payload_len;
	uint32_t ret = 0;
	
	// in fact, it has been checked
	to_put = MIN(len, buff->size - sb_len(buff));	
	if (to_put <= 0) {
		return FAILED;	// FAILED (-2)
	}
	if (to_put < len) {
		TRACE_EXCP("not enough send buffer space!"
				" mbuf_size:%d, buff_size: %d, sb_len:%d\n", 
				len, buff->size, sb_len(buff));
	}

	mbuf->payload_len = to_put;
	mbuf->tcp_seq = buff->next_seq;
	buff->next_seq += to_put;
	mbuf->mbuf_state = MBUF_STATE_TBUFFED;
	if (mbuf->priority == 0) {
		ret = mbufq_enqueue(&buff->usnd_q, mbuf);
	} else {
		ret = mbufq_enqueue(&buff->husnd_q, mbuf);
	}
	if (ret != SUCCESS) {
		TRACE_EXCP("failed to add to send buffer!\n");
		return FAILED;
	}

	mbuf_set_op(mbuf, MBUF_OP_SB_UPUT, qstack->stack_id);
	return to_put;
}

int
sb_reset_retrans(snd_buff_t buff)
{
	mbuf_t mbuf;
	if (!buff->uack_list.head) {
		TRACE_EXCP("no packet to retrans!\n");
		return FAILED;
	}
	if (buff->retrans_list.head) {
		TRACE_EXCP("the retrans_list should be empty!\n");
		return ERROR;
	}
	
	buff->retrans_list = buff->uack_list;
	buff->uack_list.head = NULL;
	buff->uack_list.tail = NULL;
	
	mbuf = buff->retrans_list.head;
	while (mbuf) {
		DSTAT_ADD(get_global_ctx()->stack_contexts[0]->retrans_snd_num, 1);
		mbuf->mbuf_state = MBUF_STATE_LOSS;
		mbuf = mbuf->buf_next;
	}
}

int
sb_remove_acked(qstack_t qstack, snd_buff_t buff, uint32_t ack_seq)
{
	mbuf_list_t list = &buff->uack_list;
	mbuf_t mbuf = list->head;
	while (mbuf && TCP_SEQ_LEQ(mbuf->tcp_seq+mbuf->payload_len, ack_seq)) {
		TRACE_MBUF("remove from uack_list\n");
		TRACE_MBUFPOOL("free the acked packet %p from core %d\n", 
				mbuf, qstack->stack_id);
		mbuf_print_info(mbuf);

		mbuf_list_pop(list);
		buff->head_seq += mbuf->payload_len;
		mbuf_free(qstack->stack_id, mbuf);
		mbuf = list->head;
		DSTAT_ADD(qstack->acked_freed, 1);
	}
	list = &buff->retrans_list;
	mbuf = list->head;
	while (mbuf && TCP_SEQ_LEQ(mbuf->tcp_seq+mbuf->payload_len, ack_seq)) {
//		TRACE_EXIT("something wrong!\n");
		TRACE_MBUF("remove from retrans_list\n");
		TRACE_MBUFPOOL("free the acked packet %p from core %d\n", 
				mbuf, qstack->stack_id);
		mbuf_print_info(mbuf);
		
		mbuf_list_pop(list);
		buff->head_seq += mbuf->payload_len;
		mbuf_set_op(mbuf, MBUF_OP_SB_FREE, qstack->stack_id);
		mbuf_free(qstack->stack_id, mbuf);
		mbuf = list->head;
		DSTAT_ADD(qstack->acked_freed, 1);
	}
	return SUCCESS;
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
