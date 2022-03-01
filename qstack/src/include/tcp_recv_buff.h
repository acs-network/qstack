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
 * @file tcp_recv_buff.h 
 * @brief receive buff for tcp streams
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 1.0
 * @detail Function list: \n
 *   1. rb_init(): alloc and init rcv_buff \n
 *   2. rb_free(): free the rcv_buff and the merged_q in it \n
 *   3. rb_put(): put a mbuf into rcv_buff \n
 *   4. rb_get(): get a mbuf from merged_q in rcv_buff \n
 *   5. rb_clear(): free all mbufs stored in the rcv_buff \n
 *   6. rb_merged_len(): calculate the length of merged data in the rcv_buff\n
 *   7. rb_print_info(): print the detailed info of receive buffer
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.28
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TCP_RECV_BUFF_H_
#define __TCP_RECV_BUFF_H_
/******************************************************************************/
#include "universal.h"
#include "mbuf_queue.h"
/******************************************************************************/
/* forward declarations */
struct tcp_recv_buffer;
struct qstack_context;
struct tcp_stream;
typedef struct tcp_stream *tcp_stream_t;
typedef struct tcp_recv_buffer *rcv_buff_t;
typedef struct qstack_context *qstack_t;
/******************************************************************************/
/* global macros */
/******************************************************************************/
/* data structures */
/*----------------------------------------------------------------------------*/
struct tcp_recv_buffer
{
	mbuf_queue merged_q;	///< ciecular queue for merged mbufs, 48B
	struct mbuf_list ooo_list;	///< simply using a list to store ofo mbufs 
#ifdef PRIORITY_RECV_BUFF
	mbuf_queue high_q;	///< list of high-priority mbufs
#endif
	
	uint32_t size;				///< max size (in seq) of total list buffer
	uint32_t init_seq;
	uint32_t head_seq;		///< start seq of merfed data 
	uint32_t merged_next;	///< seq going to be merged, multi thread safe 
};// 128B
/******************************************************************************/
/* function declarations */
/*----------------------------------------------------------------------------*/
/** 
 * init a tcp_recv_buffer instance
 *
 * @param qstack 	stack process context
 * @param buff		target rcvbuf
 * @param init_seq 	start sequence of tcp stream
 *
 * @return
 *	return rcv_buff_t if success; otherwise return NULL
 * @note 
 * 	get rb_manager_t from global mempool
 * @note
 * 	qstack is only used for core_id, ohter members should not be changed in 
 * 	this function!
 */ 
rcv_buff_t 
rb_init(qstack_t qsatck, rcv_buff_t buff, uint32_t init_seq);

/**
 * free tcp_receive_buffer and return it to mempool manager
 * 
 * @param qstack 	stack process context
 * @param buff 		target rcv_buff
 *
 * @return 
 *	return SUCCESS if success; otherwise return FAILED
 * @note
 * 	qstack is only used for core_id, ohter members should not be changed in 
 * 	this function!
 * @note
 * 	this function should not be called since the rcvbuff is a builtin struct of 
 * 	rcvvar now!
 */
uint8_t
rb_free(qstack_t qsatck, rcv_buff_t buff);
/*----------------------------------------------------------------------------*/
// mbuf access functions
/**
 * put an mbuf into rcv_buff
 *
 * @param qstack 	stack process context
 * @param stream 	current stream
 * @param mbuf 		target mbuf
 *
 * @return 
 *	return the number of new available mbuf for the user; 
 *	return ERROR if there is no rcvbuf;
 *	return FAILED if failed to add to rcvbuf
 * @note
 * 	get rb_manager_t from qstack, seq and payload from mbuf
 * @note
 * 	qstack is only used for rcv_buff_pool manager, ohter members should not 
 * 	be changed in this function!
 */
int 
rb_put(qstack_t qstack, tcp_stream_t stream, mbuf_t mbuf);

/**
 * get and remove the first merged mbuf from the receive buffer
 *
 * @param core_id	the id of the core where the function is called
 * @param stream 	current tcp stream
 *
 * @return
 *	return mbuf_t if success; otherwise return NULL
 * @note
 * 	qstack is only used for rcv_buff_pool manager, ohter members should not 
 * 	be changed in this function!
 */
mbuf_t 
rb_get(uint8_t core_id, tcp_stream_t stream);

/**
 * free all the mbufs stored in the rcv_buff
 *
 * @param qstack 	stack process context
 * @param buff 		target recv_buff
 *
 * @return null
 *
 * @note 
 * 	this function called by stack may be unsafe if the application is 
 * 	accessing mbufs from the rcv_buff 
 */ 
void 
rb_clear(qstack_t qstack, rcv_buff_t buff);
/******************************************************************************/
/* inline functions */
/**
 * calculate the length of data merged in the rcv_buff
 *
 * @param buff 	target rcv_buff
 *
 * @return 
 * 	the length of merged data;
 * @note 
 * 	it's multi-thread safe
 */
static inline uint32_t 
rb_merged_len(rcv_buff_t buff)
{
	return buff->merged_next - buff->head_seq;
}

/**
 * update init_seq, head_seq and merged_next in receive buffer
 *
 * @param buff		target receive buffer
 * @param init_seq	new irs sequence of target tcp stream
 *
 * @return null
 */
static inline void
rb_update_seq(rcv_buff_t buff, uint32_t init_seq)
{
	buff->init_seq = init_seq;
	buff->head_seq = init_seq;
	buff->merged_next = init_seq;
}

/**
 * trace detailed information of tcp receive buffer
 *
 * @param buff		target receive buffer
 *
 * @return null
 */
static inline void
rb_print_info(rcv_buff_t buff)
{
#if QDBG_BUFF
	TRACE_BUFF("receive buff state:\n head_seq:%u\t merged_next:%u\n", 
			buff->head_seq, buff->merged_next);
	TRACE_BUFF("==========merged_q\n");
	cirq_print_info(&buff->merged_q);
	#ifdef PRIORITY_RECV_BUFF
	TRACE_BUFF("==========high_q\n");
	cirq_print_info(&buff->high_q);
	#endif
	TRACE_BUFF("==========ooo_list\n");
	mbuf_list_print_info(&buff->ooo_list);
#endif
}
/******************************************************************************/
#endif //#ifdef __TCP_RECV_BUFF_H_
