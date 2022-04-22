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
 * @file tcp_stream.h
 * @brief structures and function for tcp streams
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 0.1
 * @detail Function list: \n
 *   1. tcp_stream_create(): alloc and init a tcp stream \n
 *   2. StreamHTSearch(): search the stream in hash table according to 
 *   	the quintuple \n
 *   3. tcp_stream_destroy(): destroy the tcp stream \n
 *   4. set_seq_head(): check whether the string is the head of a message\n
 *   5. set_seq_high(): check whether the message is with high priority
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
/******************************************************************************/
#ifndef __TCP_STREAM_H_
#define __TCP_STREAM_H_
/******************************************************************************/
//#define RCVBUF_DUP_CHECK
/******************************************************************************/
/* forward declarations */
struct tcp_stream;
typedef struct tcp_stream tcp_stream;
typedef tcp_stream *tcp_stream_t;
/******************************************************************************/
#include "hashtable.h"
#include "protocol.h"
#include "socket.h"
#include "tcp_recv_buff.h"
#include "tcp_send_buff.h"
#include "timestamp.h"
#include "qepoll.h"
/******************************************************************************/
/* global macros */
/******************************************************************************/
/* data structures */
struct tcp_recv_vars
{
	struct tcp_recv_buffer rcvbuf;
	/* receiver variables */
	uint32_t rcv_wnd;		/* receive window (unscaled) */
	//uint32_t rcv_up;		/* receive urgent pointer */
	uint32_t irs;			/* initial receiving sequence */
	uint32_t snd_wl1;		/* segment seq number for last window update */
	uint32_t snd_wl2;		/* segment ack number for last window update */

	/* variables for fast retransmission */
	uint8_t dup_acks;		/* number of duplicated acks */
	uint32_t last_ack_seq;	/* highest ackd seq */
	
	/* timestamps */
	uint32_t ts_recent;			/* recent peer timestamp */
	uint32_t ts_lastack_rcvd;	/* last ack rcvd time */
	uint32_t ts_last_ts_upd;	/* last peer ts update time */
	uint32_t ts_tw_expire;	// timestamp for timewait expire

	/* RTT estimation variables */
//	uint32_t srtt;			/* smoothed round trip time << 3 (scaled) */
//	uint32_t mdev;			/* medium deviation */
//	uint32_t mdev_max;		/* maximal mdev ffor the last rtt period */
//	uint32_t rttvar;		/* smoothed mdev_max */
//	uint32_t rtt_seq;		/* sequence number to update rttvar */

//	struct mbuf_list hrcvq;	/* high priority receive queue */
};
struct tcp_send_vars
{ 
	struct tcp_send_buffer sndbuf;
	uint32_t snd_una;		///< send unacknoledged
	uint32_t snd_wnd;		///< send window (unscaled)
	
	uint8_t is_wack:1, 			///< is ack for window adertisement?
			is_fin_sent:1, 
			ack_cnt:6;			///< number of acks to send. max 64

	uint8_t on_control_queue:1,	///< sender queue
			on_send_queue:1,
			on_ack_queue:1,
			on_closeq:1,
			on_resetq:1;
	/* IP-level information */
	uint16_t ip_id;

	uint16_t mss;			///< maximum segment size
	uint16_t eff_mss;		///< effective segment size (excluding tcp option)

	uint8_t wscale_mine;	///< my window scale (adertising window)
	uint8_t wscale_peer;	///< peer's window scale (advertised window)
	unsigned char *d_haddr;	///< cached destination MAC address

	/* send sequence variables */
	uint32_t peer_wnd;		///< client window size
	//uint32_t snd_up;		///< send urgent pointer (not used)
	uint32_t iss;			///< initial sending sequence
	uint32_t fss;			///< final sending sequence(reset if to be closed)

	uint8_t nif_out;		///< cached output network interface 
	/* retransmission timeout variables */
	uint8_t nrtx;			///< number of retransmission
//	uint8_t max_nrtx;		///< max number of retransmission
	uint32_t rto;			///< retransmission timeout

	/* congestion control variables */
	uint32_t cwnd;				///< congestion window
	uint32_t ssthresh;			///< slow start threshold

	/* timestamp */
	uint32_t ts_lastack_sent;	///< last ack sent time
};

enum event_list_state
{
	pdev_st_EMPTY 	= 0,
	pdev_st_WRITING 	= 1,
	pdev_st_WRITEN	= 2,
	pdev_st_READING	= 3,
	pdev_st_READ		= 4
};

struct tcp_stream
{
	//TODO: aligned to 64 Byte
//	uint32_t id:24, 
//			 stream_type:8;	// seems of no use
	uint32_t id;			// inherit from socket_id
	uint32_t snd_nxt;		/* send next */
	uint32_t rcv_nxt;		/* receive next */

	uint32_t saddr;			/* in network order */
	uint32_t daddr;			/* in network order */
	uint16_t sport;			/* in network order */
	uint16_t dport;			/* in network order */
	uint8_t	dhw_addr[6];	/* dest MAC address */
	
	uint8_t state:4;			/* tcp state */
	uint8_t close_reason:4;	/* close reason */
	volatile enum event_list_state pdev_st;
	
	int ts_rto;				/* rto timestamp */
	uint32_t last_active_ts;		/* ts_last_ack_sent or ts_last_ts_upd */
#if QDBG_OOO
	uint64_t last_req_ts;
	uint64_t last_ack_ts;
#endif
#if TCP_TIMER_RTO || TCP_TIMER_TIMEWAIT
  	TAILQ_ENTRY(tcp_stream) timer_link;	/* for rto and timewait */
#endif
#if TCP_TIMER_TIMEOUT
  	TAILQ_ENTRY(tcp_stream) timeout_link;	/* for timeout */
#endif
	int ts_tw_expire;		/* timewait timestamp */
	int16_t on_rto_idx;		/* current index in rto list */

	uint16_t on_timeout_list:1, 
			on_timewait_list:1,
			on_rcv_br_list:1, 
			on_snd_br_list:1, 
			saw_timestamp:1,	/* whether peer sends timestamp */
			sack_permit:1,		/* whether peer permits SACK */
			control_list_waiting:1, 
			have_reset:1,
			is_external:1,		/* the peer node is locate outside of lan */
			closed:1,
			is_ssl:1,
			priority:1; 
	
	struct tcp_recv_vars rcvvar;
	struct tcp_send_vars sndvar;

	int (*if_req_head)(char *str);	/* if the string is the head of a message */
	int (*if_req_high)(char *str);	/* if a message is a high-pri request */
	uint32_t next_high_pri_msg_seq;
	
	socket_t socket;
	qstack_t qstack;
	struct event_list pending_events;
	struct tcp_listener *listener;
#if REQ_STAGE_TIMESTAMP
	rs_ts_t req_stage_ts;
#endif
#ifdef RCVBUF_DUP_CHECK
	mbuf_t mbuf_last_rb_in;
#endif
};
/******************************************************************************/
/* static functions */
/******************************************************************************/
/* function declarations */
/** 
 * Description: create and init a tcp_stream instance
 * 
 * @param qstack 	stack process context
 * @param socket	the socket belongs to
 * @param type		stream type
 * @param saddr		source ip address, in network order
 * @param sport		source tcp port, in network order
 * @param daddr		dest ip address, in network order
 * @param dport		dest tcp port, in network order
 * @param seq		receive init seq
 * 
 * @return:
 *		return a tcp_stream instance if success; otherwise return NULL
 */
tcp_stream_t
tcp_stream_create(qstack_t qstack, socket_t socket, int type, uint32_t saddr, 
		uint16_t sport, uint32_t daddr, uint16_t dport, uint32_t seq);

/**
 * search the stream hahs table
 *
 * @param stack_id 		to find the hashtable the stream belong to
 * @param qtuple		the quintuple of the stream
 *
 * @return
 * 	return the stream if found in hashtable; otherwise return NULL
 */
static inline tcp_stream_t
StreamHTSearch(hashtable_t ht, qtuple_t qtuple)
{
//	return qstack->stream_socket->stream;
	return ht_search(ht, qtuple);
}

int
tcp_stream_close(int core_id, tcp_stream_t cur_stream);

void
tcp_stream_destroy(qstack_t qstack, tcp_stream_t stream);

static inline void
print_stream_key(tcp_stream_t cur_stream)
{
	TRACE_STREAM("id: %d, ip: %d, port: %d \n", 
			cur_stream->id, IP_GET_TAIL(cur_stream->daddr), 
			ntohs(cur_stream->dport));
}

void
print_stream_sndvar(tcp_stream_t cur_stream);

char *
tcp_state_to_string(const tcp_stream_t cur_stream);
/******************************************************************************/
/* inline functions */
/**
 * set the function to detect if a packet is the head of a message
 *
 * @param cur_stream	target tcp stream
 * @param func			pointer of the requst head detect function
 *
 * @return null
 *
 * @note
 *  The function input a string with type of char* (usually the start of an 
 *  mbuf payload). The function return TRUE if the string do be the head of a 
 *  message, otherwith return FALSE.
 */
static inline void
set_req_head(tcp_stream_t cur_stream, int (*func)(char*))
{
	cur_stream->if_req_head = func;
}

/**
 * set the function to check if a message is with high priority
 *
 * @param cur_stream	target tcp stream
 * @param func			pointer of the high-priority check function
 *
 * @return null
 *
 * @note
 *  The function input a message with type of char*. The function return TRUE 
 *  if the message is with high priority, otherwith return FALSE.
 * @note
 * 	If (cur_stream->if_req_head == NULL), the high-priority check function will 
 * 	check every packet's payload in default.
 */
static inline void
set_req_high(tcp_stream_t cur_stream, int (*func)(char*))
{
	cur_stream->if_req_high = func;
}
/******************************************************************************/
#endif //#ifdef __TCP_STERAM_H_
