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
 * @file tcp_out.c 
 * @brief tcp output functions
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.8.22
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DETAIL
#endif
/*----------------------------------------------------------------------------*/
#include "tcp_out.h"
#include "ip_out.h"
#include "tcp_send_buff.h"
#include "timer.h"
#include "api.h"
/******************************************************************************/
/* local macros */
#define N21Q_DEQUEUE_STRONG		// dequeue without looking up flag
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
static inline uint16_t
calculate_option_length(uint8_t flags)
{
	uint16_t optlen = 0;

	if (flags & TCP_FLAG_SYN) {
		optlen += TCP_OPT_MSS_LEN;
#if TCP_OPT_SACK_ENABLED
		optlen += TCP_OPT_SACK_PERMIT_LEN;
	#if !TCP_OPT_TIMESTAMP_ENABLED
		optlen += 2;	// insert NOP padding
	#endif /* TCP_OPT_TIMESTAMP_ENABLED */
#endif /* TCP_OPT_SACK_ENABLED */

#if TCP_OPT_TIMESTAMP_ENABLED
		optlen += TCP_OPT_TIMESTAMP_LEN;
	#if !TCP_OPT_SACK_ENABLED
		optlen += 2;	// insert NOP padding
	#endif /* TCP_OPT_SACK_ENABLED */
#endif /* TCP_OPT_TIMESTAMP_ENABLED */

		optlen += TCP_OPT_WSCALE_LEN + 1;

	} else {

#if TCP_OPT_TIMESTAMP_ENABLED
		optlen += TCP_OPT_TIMESTAMP_LEN + 2;
#endif

#if TCP_OPT_SACK_ENABLED
		if (flags & TCP_FLAG_SACK) {
			optlen += TCP_OPT_SACK_LEN + 2;
		}
#endif
	}

	assert(optlen % 4 == 0);

	return optlen;
}

static inline sender_t
get_sender(qstack_t qstack, tcp_stream_t cur_stream)
{
	return qstack->sender;
}

static inline uint8_t 
active_drop(struct tcphdr *tcph, tcp_stream_t cur_stream, uint32_t payloadlen)
{
#if ACTIVE_DROP_EMULATE
	// TODO: drop payload packets to be implemented
	if (payloadlen>1) {
		return 0;
	}
	uint32_t flag = ((uint32_t)rand()) % ACTIVE_DROP_RATE;
	if (flag) {
		/* not random selected */
		return 0;
	}
	/* this packet is random selected, check the filters */
	#ifdef DROP_UNIFORM
	return 1; 
	#else
		#ifdef DFILTER_SYN
		if (tcph->syn && !tcph->ack) {
			return 1;
		}
		#endif
		#ifdef DFILTER_SYNACK
		if (tcph->syn && tcph->ack) {
			return 1;
		}
		#endif
		#ifdef DFILTER_THIRDACK
		if (cur_stream->rcvvar.irs+1 == cur_stream->rcv_nxt && !payloadlen) {
			return 1;
		}
		#endif
		return 0;
	#endif
#else
	/* do not drop anyway */
	return 0;
#endif
}
/*----------------------------------------------------------------------------*/
static inline void
GenerateTCPTimestamp(tcp_stream *cur_stream, uint8_t *tcpopt, uint32_t cur_ts)
{
	uint32_t *ts = (uint32_t *)(tcpopt + 2);

	tcpopt[0] = TCP_OPT_TIMESTAMP;
	tcpopt[1] = TCP_OPT_TIMESTAMP_LEN;
	ts[0] = htonl(cur_ts);
	ts[1] = htonl(cur_stream->rcvvar.ts_recent);
}

static inline void
GenerateTCPOptions(tcp_stream *cur_stream, uint32_t cur_ts, 
		uint8_t flags, uint8_t *tcpopt, uint16_t optlen)
{
	int i = 0;

	if (flags & TCP_FLAG_SYN) {
		uint16_t mss;

		/* MSS option */
		mss = cur_stream->sndvar.mss;
		tcpopt[i++] = TCP_OPT_MSS;
		tcpopt[i++] = TCP_OPT_MSS_LEN;
		tcpopt[i++] = mss >> 8;
		tcpopt[i++] = mss % 256;

		/* SACK permit */
#if TCP_OPT_SACK_ENABLED
	#if !TCP_OPT_TIMESTAMP_ENABLED
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;
	#endif /* TCP_OPT_TIMESTAMP_ENABLED */
		tcpopt[i++] = TCP_OPT_SACK_PERMIT;
		tcpopt[i++] = TCP_OPT_SACK_PERMIT_LEN;
#endif /* TCP_OPT_SACK_ENABLED */

		/* Timestamp */
#if TCP_OPT_TIMESTAMP_ENABLED
	#if !TCP_OPT_SACK_ENABLED
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;
	#endif /* TCP_OPT_SACK_ENABLED */
		GenerateTCPTimestamp(cur_stream, tcpopt + i, cur_ts);
		i += TCP_OPT_TIMESTAMP_LEN;
#endif /* TCP_OPT_TIMESTAMP_ENABLED */

		/* Window scale */
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_WSCALE;
		tcpopt[i++] = TCP_OPT_WSCALE_LEN;
		tcpopt[i++] = cur_stream->sndvar.wscale_mine;

	} else {

#if TCP_OPT_TIMESTAMP_ENABLED
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;
		GenerateTCPTimestamp(cur_stream, tcpopt + i, cur_ts);
		i += TCP_OPT_TIMESTAMP_LEN;
#endif

#if TCP_OPT_SACK_ENABLED
		if (flags & TCP_OPT_SACK) {
			// TODO: implement SACK support
		}
#endif
	}

	assert (i == optlen);
}

static inline int 
ack_queue_add(qstack_t qstack, tcp_stream_t cur_stream)
{
	int ret = SUCCESS;
	if (cur_stream->sndvar.ack_cnt && !cur_stream->sndvar.on_ack_queue) {
		sender_t sender = get_sender(qstack, cur_stream);
		assert(sender != NULL);

		ret = sstreamq_enqueue(&sender->ack_queue, cur_stream);
		// ret = SUCCESS if success, or ERROR if the queue is full
		if (ret == SUCCESS) {
			cur_stream->sndvar.on_ack_queue = TRUE;
#if QDBG_OOO
			cur_stream->ackq_add_cnt++;
#endif
		} else {
			TRACE_EXCP("failed to add to ack_queue @ Stream %d\n", 
					cur_stream->id);

		}
	}
	return ret;
}
/******************************************************************************/
/** 
 * send tcp packet 
 * 
 * @param qstack stack process context
 * @param cur_stream stream to send packet
 * @param cur_ts timestamp for tcp header
 * @param flags tcp flags
 * @param mbuf mbuf to be sent, if NULL, alloc it in the function
 * @param payloadlen payload length
 * 
 * @return
 * 	return SUCCESS if send out packet;
 * 	return FAILED if failed to send (e.g. full send queue or empty mbuf)
 * 	return ERROR if error (e.g. stream not exist)
 */
static int
send_tcp_packet(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		uint8_t flags, mbuf_t mbuf, uint32_t payloadlen)
{
	struct tcphdr *tcph;
	uint16_t optlen;
	uint8_t wscale = 0;
	uint32_t window32 = 0;
	int ret = FAILED;

	optlen = calculate_option_length(flags);
	if (payloadlen > cur_stream->sndvar.mss - optlen) {
		TRACE_EXCP("Payload size exceeds MSS! "
				"paylaodlen:%d, mss: %d, optlen: %d\n", 
				payloadlen, cur_stream->sndvar.mss, optlen);
		return ERROR;
	}

	if (!mbuf) { // packet without payload
		mbuf = io_get_swmbuf(qstack->stack_id, 0);
#if MBUF_STREAMID
		mbuf->stream_id = cur_stream->id;
#endif
	}

	if(unlikely(mbuf == NULL)) {
		//get mbuf fail
		TRACE_EXCP("failed to alloc mbuf when send out tcp packet\n");
		return FAILED;
	}
	// Tx offloading
	mbuf_set_tx_offload(mbuf, ETHERNET_HEADER_LEN, IP_HEADER_LEN, 
			TCP_HEADER_LEN+optlen, mbuf->payload_len, 0, 0);

	ret = generate_ip_packet(qstack, cur_stream, mbuf, 
			TCP_HEADER_LEN + optlen + payloadlen);
	if (unlikely(ret != SUCCESS)) {
		// something wrong
		return ret;
	} else {
		tcph = mbuf_get_tcp_ptr(mbuf);
	}

//	rs_ts_add(mbuf->q_ts, REQ_ST_RSPTCPGEN);
	mbuf->payload_len = payloadlen;
	if (unlikely(tcph == NULL)) {
		return ERROR;
	}
//	memset(tcph, 0, TCP_HEADER_LEN + optlen);
	*(((uint16_t*)tcph) + 6) = 0;
	*(((uint16_t*)tcph) + 9) = 0;

	tcph->source = cur_stream->sport;
	tcph->dest = cur_stream->dport;

	if (flags & TCP_FLAG_SYN) {
		tcph->syn = TRUE;
		TRACE_CNCT("send SYN @ Stream %d, flags:%x\n", cur_stream->id, flags);
		if (cur_stream->snd_nxt != cur_stream->sndvar.iss) {
			TRACE_EXCP("Stream %d: weird SYN sequence. "
					"snd_nxt: %u, iss: %u\n", cur_stream->id, 
					cur_stream->snd_nxt, cur_stream->sndvar.iss);
		}
	}
	if (flags & TCP_FLAG_RST) {
		tcph->rst = TRUE;
	}
	if (flags & TCP_FLAG_PSH)
		tcph->psh = TRUE;

	if (flags & TCP_FLAG_WACK) {
		tcph->seq = htonl(cur_stream->snd_nxt - 1);
	} else if (flags & TCP_FLAG_FIN) {
		tcph->fin = TRUE;
		
		if (cur_stream->sndvar.fss == 0) {
			TRACE_EXCP("Stream %u: not fss set. closed: %u\n", 
					cur_stream->id, cur_stream->closed);
		}
		tcph->seq = htonl(cur_stream->sndvar.fss);
		cur_stream->sndvar.is_fin_sent = TRUE;
		TRACE_CLOSE("FIN sent @ Stream %d\n", cur_stream->id);
	} else {
		tcph->seq = htonl(cur_stream->snd_nxt);
	}
	mbuf->tcp_seq = ntohl(tcph->seq);

	if (flags & TCP_FLAG_ACK) {
		tcph->ack = TRUE;
		tcph->ack_seq = htonl(cur_stream->rcv_nxt);
		cur_stream->sndvar.ts_lastack_sent = cur_ts;
		timeout_list_update(qstack, cur_stream, cur_ts);
	}

	if (flags & TCP_FLAG_SYN) {
		wscale = 0;
	} else {
		wscale = cur_stream->sndvar.wscale_mine;
	}

	window32 = cur_stream->rcvvar.rcv_wnd >> wscale;
	tcph->window = htons((uint16_t)MIN(window32, TCP_MAX_WINDOW));
	// TODO: if the advertised window is 0, we need to advertise again later

	GenerateTCPOptions(cur_stream, cur_ts, flags, 
			(uint8_t *)tcph + TCP_HEADER_LEN, optlen);
	
	tcph->doff = (TCP_HEADER_LEN + optlen) >> 2;
	mbuf->data_len = mbuf->pkt_len = ETHERNET_HEADER_LEN + IP_HEADER_LEN + 
		TCP_HEADER_LEN+optlen + mbuf->payload_len;
	// TODO: payload process
//	tcph->check = tcp_csum((uint16_t *)tcph, 
//			TCP_HEADER_LEN + optlen + payloadlen, 
//		    cur_stream->saddr, cur_stream->daddr);
    if(IF_TX_CHECK){
	    tcph->check = 0;
	}
	else
	{
	    tcph->check = 0;
	    tcph->check = tcp_csum((uint16_t *)tcph, 
			TCP_HEADER_LEN + optlen + payloadlen, 
		    cur_stream->saddr, cur_stream->daddr);
	}
	cur_stream->snd_nxt += payloadlen;

	if (tcph->syn || tcph->fin) {
		cur_stream->snd_nxt++;
		payloadlen++;
	}

	if (mbuf->mbuf_state = MBUF_STATE_TBUFFED) { // data packet first to be sent
		mbuf->mbuf_state = MBUF_STATE_SENT;
	} else if (mbuf->mbuf_state = MBUF_STATE_LOSS) { // retrans pkt
		mbuf->mbuf_state = MBUF_STATE_RETRANS;
	} else if (mbuf->mbuf_state = MBUF_STATE_FREE) { // ack or control packet
		mbuf->mbuf_state = MBUF_STATE_SENT;
	}

	TRACE_CHECKP("tcp packet out @ Stream %d, pkt:%p, seq: %u, ack: %u "
			"payload_len: %d priority:%d syn:%d, rst:%d\n",        
			cur_stream->id, mbuf, mbuf->tcp_seq, cur_stream->rcv_nxt, 
			payloadlen, mbuf->priority, tcph->syn, tcph->rst);
	mbuf_print_detail(mbuf);

	// TODO: multi-core and multi-priority mode support
	if (payloadlen) {
		rs_ts_add(mbuf->q_ts, REQ_ST_RSPOUT);
//		rs_ts_check(mbuf);
		if (cur_stream->state > TCP_ST_ESTABLISHED) {
			TRACE_CLOSE("Payload after ESTABLISHED: length: %d, snd_nxt: %u\n", 
					payloadlen, cur_stream->snd_nxt);
		}

		/* update retransmission timer if have payload or SYN/FIN */
		rto_list_add(qstack, cur_stream, cur_ts);
	}
#if QDBG_OOO
	else {
		cur_stream->ack_out_cnt++;
	}
	cur_stream->last_ack_ts = get_time_ns();
#endif
	if (tcph->syn) {
		if (tcph->ack) {
			TRACE_CNCT("syn ack packet with seq %u was sent @ Stream %d\n", 
					mbuf->tcp_seq, cur_stream->id);
		} else {
			TRACE_CNCT("syn packet with seq %u was sent @ Stream %d\n", 
					mbuf->tcp_seq, cur_stream->id);
		}
	}
	if (tcph->fin) {
		if (tcph->ack) {
			TRACE_CLOSE("fin ack packet with seq %u was sent @ Stream %d\n", 
					mbuf->tcp_seq, cur_stream->id);
		} else {
			TRACE_CLOSE("fin packet with seq %u was sent @ Stream %d\n", 
					mbuf->tcp_seq, cur_stream->id);
		}
	}

	mbuf_set_op(mbuf, MBUF_OP_SND_SPUT, qstack->stack_id);
	if (active_drop(tcph, cur_stream, payloadlen)) {
		/* actively drop this packet for emulation*/
		if (payloadlen<2) {
			TSTAT_ADD(1,qstack->stack_id, 1);
			mbuf_free(qstack->stack_id, mbuf);
		} else {
			TRACE_TODO();
		}
	}
	io_send_mbuf(qstack, 1, mbuf, mbuf->data_len);
	return ret;
}
/*----------------------------------------------------------------------------*/
// send control, data, ack packets 
/**
 * send control tcp packets
 * 
 * @param qstack 		stack process context
 * @param cur_stream 	stream ready to send control packet
 * @param cur_ts 		timestamp for tcp header
 * 
 * @return
 * 	return SUCCESS if successfully send out tcp control packet;
 * 	return FAILED if don't need to send control packet;
 * 	return ERROR if something wrong
 */
static inline int 
send_tcp_control_packet(qstack_t qstack, tcp_stream_t cur_stream, 
		uint32_t cur_ts)
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	int ret = FAILED;

	if (cur_stream->state == TCP_ST_SYN_SENT) {
		/* Send SYN here */
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_SYN, NULL, 
				0);

	} else if (cur_stream->state == TCP_ST_SYN_RCVD) {
		/* Send SYN/ACK here */
		cur_stream->snd_nxt = sndvar->iss;
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
				TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);

	} else if (cur_stream->state == TCP_ST_ESTABLISHED) {
		/* Send ACK here */
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_ACK, NULL, 
				0);

	} else if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
		/* Send ACK for the FIN here */
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_ACK, NULL, 
				0);

	} else if (cur_stream->state == TCP_ST_LAST_ACK) {
		/* if it is on ack_list, send it after sending ack */
		// TODO:
//		if (sndvar->on_send_queue || sndvar->on_ack_queue) {
//			ret = FAILED;
//		} else {
			/* Send FIN/ACK here */
			ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
					TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
//		}
	} else if (cur_stream->state == TCP_ST_FIN_WAIT_1) {
		/* if it is on ack_list, send it after sending ack */
		// TODO:
//		if (sndvar->on_send_queue || sndvar->on_ack_queue) {
//			ret = FAILED;
//		} else {
			/* Send FIN/ACK here */
			ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
					TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
//		}

	} else if (cur_stream->state == TCP_ST_FIN_WAIT_2) {
		/* Send ACK here */
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_ACK, NULL, 
				0);

	} else if (cur_stream->state == TCP_ST_CLOSING) {
		if (sndvar->is_fin_sent) {
			/* if the sequence is for FIN, send FIN */
			if (cur_stream->snd_nxt == sndvar->fss) {
				ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
						TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
			} else {
				ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
						TCP_FLAG_ACK, NULL, 0);
			}
		} else {
			/* if FIN is not sent, send fin with ack */
			ret = send_tcp_packet(qstack, cur_stream, cur_ts, 
					TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
		}

	} else if (cur_stream->state == TCP_ST_TIME_WAIT) {
		/* Send ACK here */
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_ACK, NULL, 
				0);

	} else if (cur_stream->state == TCP_ST_CLOSED) {
		/* Send RST here */
		TRACE_EXCP("Stream %d: Try sending RST (TCP_ST_CLOSED)\n", 
				cur_stream->id);
		/* first flush the data and ack */
		// TODO:
//		if (sndvar->on_send_queue || sndvar->on_ack_queue) {
//			ret = FAILED;
//		} else {
//			ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_RST, 
//					NULL, 0);
//			if (ret == SUCCESS) {
//				tcp_stream_destroy(qstack, cur_stream);
//			}
//		}
	}

	return ret;
}

/**
 * send out mbuf buffered in the tcp tream's send buffer
 *
 * @param qstack		stack process context
 * @param cur_stream	target tcp stream
 * @param cur_ts		timestamp for tcp header
 *
 * @return
 * 	return the number of mbufs sent out
 */
static inline int
flush_tcp_sending_buffer(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	snd_buff_t buff = &sndvar->sndbuf;
	const uint32_t maxlen = sndvar->mss - calculate_option_length(TCP_FLAG_ACK);
	uint8_t *data = NULL;
	uint32_t buffered_len;
	uint32_t seq = 0;
	uint32_t len = 0;
	int16_t ret;
	uint32_t window = 0;
	int packets = 0;
	mbuf_t mbuf = NULL;
	uint8_t empty_flag = 1;

	if (unlikely(cur_stream->sndvar.is_fin_sent)) {
		TRACE_EXCP("try to send data pkt after fin sent @ Stream %d\n", 
				cur_stream->id);
		return 0;
	}
	if (unlikely(sb_len(buff) == 0)) {
		return 0;
	}
	// check if the state of send buffer is currect
	TRACE_BUFF("snd_nxt:%u\t cwnd:%u\t snd_una:%u\n", 
			cur_stream->snd_nxt, sndvar->cwnd, sndvar->snd_una);
	sb_print_info(buff);

	window = MIN(sndvar->cwnd, sndvar->peer_wnd);
	// make sure window is availalbe for at least one packet
	window = MAX(window, SEND_WINDOW_MIN);	

	while (1) {
		seq = cur_stream->snd_nxt;

		if (unlikely(TCP_SEQ_LT(seq, buff->head_seq))) {
			TRACE_EXCP("Invalid sequence to send @ Stream %d!"
					" seq: %u, buff_head: %u\n", 
					cur_stream->id, seq, buff->head_seq);
			print_stream_key(cur_stream);
			print_stream_sndvar(cur_stream);
			break;
		}

		//try to send out retrans packets first
		mbuf = mbuf_list_pop(&buff->retrans_list);

		// if there is no packet to be retrans, send out unprocessed pkts
		if (!mbuf) {
			// send high-priority packets first
			mbuf = mbufq_dequeue(&buff->husnd_q); 
			if (!mbuf) {
				mbuf = mbufq_dequeue(&buff->usnd_q);
			}
			if (mbuf) {
				empty_flag = 0;
				BSTAT_ADD(qstack->resp_num, 1);
			}
		} else {
			if (seq != mbuf->tcp_seq) {
				TRACE_EXCP("Retrans mbuf seq invalid @ Stream %d! "
						"mbuf_seq: %u, snd_nxt:%u\n", 
						cur_stream->id, mbuf->tcp_seq, seq);
			}
			TRACE_OOO("get mbuf from retrans queue @ Stream %d!, seq: %u, len: %d\n", 
					cur_stream->id, mbuf->tcp_seq, mbuf->payload_len);
		}
		if (mbuf) {
#if MBUF_STREAMID
			if (mbuf->stream_id != cur_stream->id) {
				TRACE_ERR("the mbuf %p from Stream %d is with stream_id %d\n", 
						mbuf, cur_stream->id, mbuf->stream_id);
			}
#endif
			rs_ts_add(mbuf->q_ts, REQ_ST_RSPGET);
			len = mbuf->payload_len;
			if (unlikely(!len)) {
				mbuf_free(qstack->stack_id, mbuf);
				TRACE_EXCP("0 payload when flushing send buff @ Stream %d\n", 
						cur_stream->id);
				TRACE_MBUFPOOL("free the packet %p with 0 payload from core %d", 
						mbuf, qstack->stack_id);
				continue;
			}
		}
		else {
			if (empty_flag) {
//				TRACE_EXCP("unexcepted empty send buffer @ Stream %d\n", 
//						cur_stream->id);
				DSTAT_ADD(qstack->check_point_1, 1);
			}
			break;
		}

//		if (seq - sndvar->snd_una + len > window) 
		if (unlikely(TCP_SEQ_GT(seq+len, sndvar->snd_una+window)))
		{
			/* Ask for new window advertisement to peer */
			TRACE_EXCP("out of send window! @ Stream %d,"
					" seq: %u, snd_una: %u, payload_len:%u, window:%u\n", 
					cur_stream->id, seq, sndvar->snd_una, len, window);
			if (seq - sndvar->snd_una + len > sndvar->peer_wnd) {
				if (TS_TO_MSEC(cur_ts - sndvar->ts_lastack_sent) > 500) {
					enqueue_ack(qstack, cur_stream, ACK_OPT_WACK);
				}
			}
//			mbuf_free(qstack->stack_id, mbuf);
			send_event_add(qstack, qstack->stack_id, cur_stream, mbuf->priority);
			return packets;
		}
	
		if (unlikely(mbuf->payload_len && mbuf->tcp_seq != cur_stream->snd_nxt)) {
			TRACE_OOO("unexcept seq of mbuf to be sent @ Stream %d\n"
					" payload_len: %d, seq: %u, snd_nxt:%u, mbuf_streamid: %d\n", 
					cur_stream->id, mbuf->payload_len, mbuf->tcp_seq, 
					cur_stream->snd_nxt, mbuf->stream_id);
		}
		// io_send_mbuf() is called in send_tcp_packet()
		ret = send_tcp_packet(qstack, cur_stream, cur_ts, TCP_FLAG_ACK, mbuf, 
				len);
		// snd_nxt has been updated
		if (ret < 0) {
			TRACE_EXIT("failed to send out tcp packet!\n");
			mbuf_free(qstack->stack_id, mbuf);
			return ret;
		}
		mbuf_list_append(&buff->uack_list, mbuf);
		packets++;
	}
	return packets;
}

static inline tcp_stream_t
get_from_send_queue(qstack_t qstack)
{
	tcp_stream_t ret = streamq_dequeue(&qstack->send_hevent_queue);
	if (ret) {
		return ret;
	} else {
		cirq_t q = &qstack->send_event_queue.queues[1];
		if (!cirq_empty(q)) {
			TRACE_SENDQ("get from send queue %p, tail %d\n",            
					&q->queue[q->tail], q->tail);
		}
		return streamq_dequeue(&qstack->send_event_queue);
	}

}

/**
 * send an mbuf to a stream's send buffer
 *
 * @param qstack		stack process context
 * @param cur_stream	target tcp stream
 * @param mbuf			target mbuf
 *
 * @return
 * 	return the length of data sent to send buffer if success, otherwise return 
 * 	FAILED or ERROR
 */
static inline int 
send_from_user(qstack_t qstack, tcp_stream_t cur_stream, mbuf_t mbuf)
{
	struct tcp_send_vars *sndvar = &cur_stream->sndvar;
	int sndlen;
	int ret = FAILED;

	sndlen = MIN((int)sndvar->snd_wnd, mbuf->payload_len);
	if (sndlen <= 0) {
		errno = EAGAIN;
		return FAILED;
	}

	/* allocate send buffer if not exist */
	ret = sb_put(qstack, &sndvar->sndbuf, mbuf);
	sndvar->snd_wnd = sndvar->sndbuf.size - sb_len(&sndvar->sndbuf);
	if (ret <= 0) {
		TRACE_EXCP("sb_put() failed\n");
		return FAILED; // FAILED (-2)
	}
	
	if (sndvar->snd_wnd <= 0) {
		TRACE_EXCP("Sending buffer became full!! snd_wnd: %u\n", 
				sndvar->snd_wnd);
	}

	return ret;
}
/******************************************************************************/
/* functions */
int 
control_queue_add(qstack_t qstack, tcp_stream_t cur_stream)
{
	int ret = SUCCESS;
	if (!cur_stream->sndvar.on_control_queue) {
		sender_t sender = get_sender(qstack, cur_stream);
		assert(sender != NULL);

		ret = sstreamq_enqueue(&sender->control_queue, cur_stream);
		// ret = SUCCESS if success, or ERROR if the queue is full
		if (ret == SUCCESS) {
			cur_stream->sndvar.on_control_queue = TRUE;
		}
	}
	return ret;
}

int
enqueue_ack(qstack_t qstack, tcp_stream_t cur_stream, uint8_t opt)
{
	if (!(cur_stream->state == TCP_ST_ESTABLISHED || 
			cur_stream->state == TCP_ST_CLOSE_WAIT || 
			cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_FIN_WAIT_2)) {
		TRACE_EXCP("wrong stream state when enqueue ack: %d\n", 
				cur_stream->state);
	}

	if (opt == ACK_OPT_NOW) {
		if (cur_stream->sndvar.ack_cnt < 63) { // avoid bitfild overflow
			cur_stream->sndvar.ack_cnt++;
		}
	} else if (opt == ACK_OPT_AGGREGATE) {
		if (cur_stream->sndvar.ack_cnt == 0) {
			cur_stream->sndvar.ack_cnt = 1;
		}
	} else if (opt == ACK_OPT_WACK) {
		cur_stream->sndvar.is_wack = TRUE;
	}
	return ack_queue_add(qstack, cur_stream);
}

int 
_q_tcp_send(int core_id, int sockid, mbuf_t mbuf, uint32_t len, uint8_t flags)
{
	int ret;
	qstack_t qstack;
	tcp_stream_t cur_stream;
	struct tcp_send_vars *sndvar;

	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	if (!cur_stream) {
		TRACE_EXCP("no available stream in socket!");
		errno = EBADF;
		return -1;
	}
	qstack = cur_stream->qstack;
	
	if (!mbuf) {
		TRACE_EXCP("try to send NULL mbuf!\n");
		return -1;
	}
	if (mbuf->mbuf_state != MBUF_STATE_TALLOC) {
		mbuf_print_detail(mbuf);
		TRACE_ERROR("dupli-send mbuf @ Socket %d! mbuf_state: %d\n", 
				sockid, mbuf->mbuf_state);
	}
	mbuf->payload_len = len;
	mbuf->buf_next = NULL;
#if MBUF_STREAMID
	mbuf->stream_id = cur_stream->id;
#endif
	if (flags & QFLAG_SEND_HIGHPRI) {
		mbuf->priority = 1;
	} else {
		mbuf->priority = 0;
	}

	if (!cur_stream || 
			!(cur_stream->state == TCP_ST_ESTABLISHED || 
			  cur_stream->state == TCP_ST_CLOSE_WAIT)) {
		TRACE_EXCP("q_write stream error, state = %d\n", cur_stream->state);
		return ERROR;
	}

	if (len <= 0) {
		TRACE_EXCP("q_write error payload_len\n");
		return -1;
	}

	sndvar = &cur_stream->sndvar;

	ret = send_from_user(qstack, cur_stream, mbuf);
	if (ret > 0) {
		send_event_add(qstack, core_id, cur_stream, mbuf->priority);
	} else {
		TRACE_EXCP("failed to add to send_buffer @ Stream %d!\n", 
				cur_stream->id);
		q_free_mbuf(core_id, mbuf);
	}

	/* if there are remaining sending buffer, generate write event */
//	if (sndvar->snd_wnd > 0) {
//		// TODO: raise write event to user
//	}

	rs_ts_pass_from_stream(cur_stream, mbuf);
	rs_ts_add(mbuf->q_ts, REQ_ST_RSPWRITE);
	return ret;
}
/*----------------------------------------------------------------------------*/
int 
write_tcp_control_queue(qstack_t qstack, uint32_t cur_ts)
{
	tcp_stream_t cur_stream;
	sender_t sender = qstack->sender;
	stream_queue_t control_queue = &sender->control_queue;
	int cnt = 0;
	int ret = FAILED;

	while (cnt < MAX_CTRL_PKT_BATCH && 
			(cur_stream = sstreamq_dequeue(control_queue)) != NULL) {
		if (cur_stream->sndvar.on_control_queue) {
			cur_stream->sndvar.on_control_queue = FALSE;
			ret = send_tcp_control_packet(qstack, cur_stream, cur_ts);		  
			if (ret == SUCCESS) 
			{ 
				cnt++;
			} //else if (ret == FALSE) { // only when mbuf alloc failed
			//	TRACE_EXCP("Stream %d: RST send failure\n");
			//	cur_stream->sndvar.on_control_queue = TRUE;
			//	sstreamq_enqueue(control_queue, cur_stream);
			//}
		} else {
			TRACE_EXCP("Stream %d: not on control list.\n", cur_stream->id);
		}
	}
	return cnt;
}

int 
write_tcp_send_queue(qstack_t qstack, uint32_t cur_ts)
{
	tcp_stream_t cur_stream;
//	stream_queue_t send_queue = &sender->send_queue;
//	stream_queue_t send_queue = &qstack->send_event_queue;
	int cnt = 0;
	int ret = FAILED;

	/* Send data */
	while (cnt < MAX_DATA_PKT_BATCH && 
			(cur_stream = get_from_send_queue(qstack)) != NULL) {
		ret = FAILED;
		TRACE_SENDQ("write send queue @ Stream %d with state %d\n", 
				cur_stream->id, cur_stream->state);

		/* Send data here */
		/* Only can send data when ESTABLISHED or CLOSE_WAIT */
		if (likely(cur_stream->state == TCP_ST_ESTABLISHED)) {
//			if (unlikely(cur_stream->sndvar.on_control_queue)) {
//				/* delay sending data after until on_control_list becomes off */
//				ret = FAILED;
//			} else {
				ret = flush_tcp_sending_buffer(qstack, cur_stream, cur_ts);
//			}
		} else if (cur_stream->state == TCP_ST_CLOSE_WAIT || 
				cur_stream->state == TCP_ST_FIN_WAIT_1 || 
				cur_stream->state == TCP_ST_LAST_ACK) {
			ret = flush_tcp_sending_buffer(qstack, cur_stream, cur_ts);
		} 
		if (ret > 0) {
			cnt+= ret;
		}

		/* the ret value is the number of packets sent. */
		if (ret <= 0) {
			control_queue_add(qstack, cur_stream);
			/* since there is no available write buffer, break */
			break;
		} else {
			cur_stream->sndvar.on_send_queue = FALSE;
			/* decrease ack_cnt for the piggybacked acks */
			if (cur_stream->sndvar.ack_cnt > 0) {
				if (cur_stream->sndvar.ack_cnt > ret) {
					cur_stream->sndvar.ack_cnt -= ret;
				} else {
					cur_stream->sndvar.ack_cnt = 0;
				}
			}
			if (cur_stream->control_list_waiting) {
				if (!cur_stream->sndvar.on_ack_queue) {
					cur_stream->control_list_waiting = FALSE;
					control_queue_add(qstack, cur_stream);
				}
			}
		}
	}
	return cnt;
}

int 
write_tcp_ack_queue(qstack_t qstack, uint32_t cur_ts)
{
	tcp_stream_t cur_stream;
	sender_t sender = qstack->sender;
	stream_queue_t ack_queue = &sender->ack_queue;
	int to_ack;
	int cnt = 0;		///< num of packets sent out
	int ret = FAILED;

	/* Send aggregated acks */
	while (cnt < MAX_ACK_PKT_BATCH && 
			(cur_stream = sstreamq_dequeue(ack_queue)) != NULL) {
#if QDBG_OOO
		cur_stream->ackq_get_cnt++;
#endif
		if (cur_stream->sndvar.on_ack_queue) {
			cur_stream->sndvar.on_ack_queue = FALSE;
			/* this list is only to ack the data packets */
			/* if the ack is not data ack, then it will not process here */
			to_ack = FALSE;
			if (cur_stream->state == TCP_ST_ESTABLISHED || 
					cur_stream->state == TCP_ST_CLOSE_WAIT || 
					cur_stream->state == TCP_ST_FIN_WAIT_1 || 
					cur_stream->state == TCP_ST_FIN_WAIT_2 || 
					cur_stream->state == TCP_ST_TIME_WAIT) {
				/* TIMEWAIT is possible since the ack is queued 
				   at FIN_WAIT_2 */
				// TODO: rcv_nxt check
//				if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, 
//							cur_stream->rcvvar.rcvbuf.head_seq + 
//							cur_stream->rcvvar.rcvbuf.merged_len)) {
					to_ack = TRUE;
//				}
			} else {
				TRACE_EXCP("Try sending ack at improper state %d @ Stream%d\n",
						cur_stream->state, cur_stream->id);
			}

			if (to_ack) {
				/* send the queued ack packets */
				while (cur_stream->sndvar.ack_cnt > 0) {
					ret = send_tcp_packet(qstack, cur_stream, 
							cur_ts, TCP_FLAG_ACK, NULL, 0);
					if (ret != SUCCESS) {
						/* since there is no available write buffer, break */
						TRACE_EXCP("failed to generate ack packet @ Stream %d\n", 
								cur_stream->id);
						break;
					} else {
						cnt++;
					}
					cur_stream->sndvar.ack_cnt--;
				}

				/* if is_wack is set, send packet to get window advertisement */
				if (cur_stream->sndvar.is_wack) {
					cur_stream->sndvar.is_wack = FALSE;
					ret = send_tcp_packet(qstack, cur_stream, 
							cur_ts, TCP_FLAG_ACK | TCP_FLAG_WACK, NULL, 0);
					if (ret != SUCCESS) {
						/* since there is no available write buffer, break */
						TRACE_EXCP("failed to generate wack packet @ Stream %d\n", 
								cur_stream->id);
						cur_stream->sndvar.is_wack = TRUE;
					} else {
						cnt++;
					}
				}

			} else {
				TRACE_EXCP("to_ack is 0 @ Stream %d\n", 
						cur_stream->id);
				cur_stream->sndvar.on_ack_queue = FALSE;
				cur_stream->sndvar.ack_cnt = 0;
				cur_stream->sndvar.is_wack = 0;
			}

			if (cur_stream->control_list_waiting) {
				if (!cur_stream->sndvar.on_send_queue) {
					cur_stream->control_list_waiting = FALSE;
					control_queue_add(qstack, cur_stream);
				}
			}
		} else {
			TRACE_EXCP("Stream %d: not on ack list.\n", cur_stream->id);
		}
	}
	return cnt;
}
/*----------------------------------------------------------------------------*/
int
send_tcp_packet_standalone(qstack_t qstack, uint32_t saddr, uint16_t sport, 
		uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack_seq, 
		uint16_t window, uint8_t flags, uint32_t cur_ts, uint32_t echo_ts)
{
	struct tcphdr *tcph;
	uint8_t *tcpopt;
	uint32_t *ts;
	uint16_t optlen;
	int rc = -1;
	mbuf_t mbuf;

	TRACE_EXIT("unexcepted to send tcp packet stand alone!\n");
	mbuf = io_get_swmbuf(qstack->stack_id, 0);
	if (!mbuf) {
		TRACE_EXCP("failed to alloc mbuf!\n");
		return ERROR;
	} else {
		TRACE_MBUFPOOL("alloc mbuf %p @ Stack %d for ack\n", 
				mbuf, qstack->stack_id);
	}

	
	optlen = calculate_option_length(flags);
	generate_ip_packet_standalone(qstack, mbuf, IPPROTO_TCP, 0, saddr, daddr, 
			TCP_HEADER_LEN + optlen);
	mbuf->tcp_seq = seq;
	mbuf->payload_len = 0;
	mbuf->l4_len = (TCP_HEADER_LEN + optlen);
	mbuf->data_len = mbuf->pkt_len = mbuf->l2_len + mbuf->l3_len + 
			mbuf->l4_len + mbuf->payload_len;

	tcph = mbuf_get_tcp_ptr(mbuf);
	if (tcph == NULL) {
		return ERROR;
	}
//	memset(tcph, 0, TCP_HEADER_LEN + optlen);
	*(((uint16_t*)tcph) + 6) = 0;
	*(((uint16_t*)tcph) + 9) = 0;

	tcph->source = sport;
	tcph->dest = dport;

	if (flags & TCP_FLAG_SYN)
		tcph->syn = TRUE;
	if (flags & TCP_FLAG_FIN)
		tcph->fin = TRUE;
	if (flags & TCP_FLAG_RST)
		tcph->rst = TRUE;
	if (flags & TCP_FLAG_PSH)
		tcph->psh = TRUE;

	tcph->seq = htonl(seq);
	if (flags & TCP_FLAG_ACK) {
		tcph->ack = TRUE;
		tcph->ack_seq = htonl(ack_seq);
	}

	tcph->window = htons(MIN(window, TCP_MAX_WINDOW));

	tcpopt = (uint8_t *)tcph + TCP_HEADER_LEN;
	ts = (uint32_t *)(tcpopt + 4);

	tcpopt[0] = TCP_OPT_NOP;
	tcpopt[1] = TCP_OPT_NOP;
	tcpopt[2] = TCP_OPT_TIMESTAMP;
	tcpopt[3] = TCP_OPT_TIMESTAMP_LEN;
	ts[0] = htonl(cur_ts);
	ts[1] = htonl(echo_ts);

	tcph->doff = (TCP_HEADER_LEN + optlen) >> 2;	
//	tcph->check = tcp_csum((uint16_t *)tcph, TCP_HEADER_LEN + optlen, 
//			saddr, daddr);
    if(IF_TX_CHECK){
	    tcph->check = 0;
	}
	else
	{
	    tcph->check = 0;
	    tcph->check = tcp_csum((uint16_t *)tcph, TCP_HEADER_LEN + optlen, 
			saddr, daddr);
	}


	mbuf->mbuf_state = MBUF_STATE_SENT;
	io_send_mbuf(qstack, 0, mbuf, mbuf->data_len);
	return SUCCESS;
}
/******************************************************************************/
