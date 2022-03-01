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
 * @file tcp_send_buff.h 
 * @brief tcp_send_buff management
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 0.1
 * @detail Function list: \n
 *   1. sb_init(): init the send buff\n
 *   2. sb_free(): clear the contents in the send buf\n
 *   3. sb_len(): get the length of stored unacked data in the send buff\n
 *   4. sb_put(): put an mbuf into send buff\n
 *   5. sb_remove_acked(): remove acked mbufs in the send buff\n
 *   6. sb_reset_retrans(): reset the unacked mbufs to retrans state\n
 *   7. sb_print_info(): print the detailed information of send buff
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.24 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TCP_SEND_BUFF_H_
#define __TCP_SEND_BUFF_H_
/******************************************************************************/
/* forward declarations */
struct tcp_send_buffer;
struct qstack_context;
struct tcp_stream;
typedef struct tcp_stream *tcp_stream_t;
typedef struct tcp_send_buffer *snd_buff_t;
typedef struct qstack_context *qstack_t;
/******************************************************************************/
#include "universal.h"
#include "mbuf_queue.h"
/******************************************************************************/
/* global macros */
/******************************************************************************/
/* data structures */
struct tcp_send_buffer
{
	mbuf_queue usnd_q;	///< mbufs put by user and not processed 
	mbuf_queue husnd_q;	///< high-priority send queue
	struct mbuf_list uack_list;		///< mbufs have been sent out but not acked
	struct mbuf_list retrans_list;	///< mbufs to be retransed

	uint32_t size;		///< max size of send buffer
	uint32_t head_seq;	///< tcp sequence of first unacked mbuf
	uint32_t next_seq;	///< tcp sequence for next mbuf put in by user
}; // 144B 
/******************************************************************************/
/* function declarations */
/**
 * init a send buffer
 *
 * @param qstack 		stack process context
 * @param buff			target sndbuf
 * @param init_seq 		init sequence for send buffer
 *
 * @return
 * 	return an allocated send buffer if success, otherwise return NULL
 * @note
 * 	init_seq should be equal to sndvar->iss
 */
snd_buff_t
sb_init(qstack_t qstack, snd_buff_t buff, uint32_t init_seq);

/**
 * clear the contents in the sndbuf (do not free the struct itself)
 *
 * @param qstack 	target qstacg
 * @param buff		target send buff
 *
 * @return null
 */
int
sb_free(qstack_t qstack, snd_buff_t buff);

/**
 * calculate the lenngth of data stored in the snd_buff (data not acked)
 *
 * @param buff		target send buffer
 *
 * @return
 * 	return the length of data stored in the snd_buff
 */
static inline uint32_t
sb_len(snd_buff_t buff)
{
	return buff->next_seq - buff->head_seq;
}

/**
 * put am mbuf into send buffer
 *
 * @param qstack	stack process context
 * @param buff		target send buffer
 * @param mbuf		target mbuf
 *
 * @return
 * 	return the length of data put into the send buffer if success; otherwise
 * 	return FAILED or ERROR
 */
int
sb_put(qstack_t qstack, snd_buff_t buff, mbuf_t mbuf);

/**
 * remove acked mbufs in the uack_list and retrans_list
 *
 * @param qstack		stack process context
 * @param buff			target send buffer
 * @param ack_seq		acked sequence
 *
 * @return
 * 	return SUCCESS ifsuccess; otherwise return FAILED or ERROR
 */
int 
sb_remove_acked(qstack_t qstack, snd_buff_t buff, uint32_t ack_seq);

/**
 * reset mbufs in the uack_list into retrans_list
 *
 * @param buff		target send buffer
 *
 * @return
 * 	return SUCCESS if success; return FAILED if the uack_list is empty;
 * 	return ERROR if something wrong
 */
int 
sb_reset_retrans(snd_buff_t buff);

/**
 * trace detailed information of tcp send buffer
 *
 * @param buff		target send buffer
 *
 * @return null
 */
static inline void
sb_print_info(snd_buff_t buff)
{
#if QDBG_BUFF
	TRACE_BUFF("send buff state:\n head_seq:%u\t next_seq:%u\n", 
			buff->head_seq, buff->next_seq);
	TRACE_BUFF("==========usnd_q\n");
	cirq_print_info(&buff->usnd_q);
	TRACE_BUFF("==========husnd_q\n");
	cirq_print_info(&buff->husnd_q);
	TRACE_BUFF("==========uack_list\n");
	mbuf_list_print_info(&buff->uack_list);
	TRACE_BUFF("==========retrans_list\n");
	mbuf_list_print_info(&buff->retrans_list);
#endif
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef __TCP_SEND_BUFF_H_
