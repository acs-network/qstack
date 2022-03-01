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
 * @file icmp.c
 * @brief structures and functions for ICMP
 * @author Guo Ziting (guoziting@ict.ac.cn)
 * @date 2018.8.21
 * @version 1.0
 * @detail Function list: \n
 *   1. request_icmp(): send ICMP request with given parameters \n
 *   2. process_icmp_packet(): process ICMP packet \n
 *	 3. dump_icmp_packet(): dump ICMP packet \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.21
 *   	Author: Guo Ziting
 *   	Modification: create
 *   2. Date: 2018.8.22
 *   	Author: Guo Ziting
 *   	Modification: add request_icmp(), icmp_output(), process_icmp_packet(),
 *			process_icmp_echo_request() and dump_icmp_packet() functions.
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif
/*----------------------------------------------------------------------------*/
#include "universal.h"
#include "qstack.h"
#include "icmp.h"
#include "eth_out.h"
#include "ip_in.h"
#include "ip_out.h"
#include "debug.h"
#include "arp.h"
/******************************************************************************/
/* local macros */
#define IP_NEXT_PTR(iph) ((uint8_t *)iph + (iph->ihl << 2))
/******************************************************************************/
/* forward declarations */
void
dump_icmp_packet(struct icmphdr *icmph, uint32_t saddr, uint32_t daddr);
void
request_icmp(qstack_t qstack, uint32_t saddr, uint32_t daddr,
	uint16_t icmp_id, uint16_t icmp_sequence,
	uint8_t *icmpd, uint16_t len);
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
/**
 * Calculate ICMP checksum
 * @param icmph ICMP header
 * @param len length of ICMP packet
 * @return
 *		return ICMP checkum
 */
static uint16_t
icmp_checksum(uint16_t *icmph, int len)
{
  assert(len >= 0); // Why need this?

  uint16_t ret = 0;
  uint32_t sum = 0;
  uint16_t odd_byte = 0;

  while (len > 1) {
    sum += *icmph++;
    len -= 2;
  }

  if (len == 1) {
    /* code */
    *(uint8_t*)(&odd_byte) = * (uint8_t*)icmph;
    sum += odd_byte;
  }

  sum =  (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  ret =  ~sum;

  return ret;
}
/*----------------------------------------------------------------------------*/
/**
 * output ICMP packet
 * @param qstack stack process context
 * @param saddr sender IP address
 * @param daddr destination IP address
 * @param icmp_type ICMP type
 * @param icmp_code ICMP code
 * @param icmp_id ICMP ID
 * @param icmp_seq ICMP sequence
 * @param icmpd ICMP data
 * @param len length of ICMP data
 * @return
 *		return SUCCESS if success, else return ERROR
 */
static int
icmp_output(qstack_t qstack, uint32_t saddr, uint32_t daddr,
		uint8_t icmp_type, uint8_t icmp_code, uint16_t icmp_id,
		uint16_t icmp_seq, uint8_t *icmpd, uint16_t len)
{
	struct icmphdr *icmph;
	struct rte_mbuf *mbuf; // mbuf to be sent
    int ret;
    int pkt_len;

	// NOTE: generate_ip_packet_standalone() defined in ip_out.c
    mbuf = io_get_swmbuf(qstack->stack_id, 0);
    ret = generate_ip_packet_standalone(qstack, mbuf, IPPROTO_ICMP, 0, saddr, daddr, sizeof(struct icmphdr) + len);
    if (!ret) {
        return ERROR;
    }
	icmph = (struct icmphdr *)mbuf_get_icmphdr_ptr(mbuf);
	if (!icmph) {
        return ERROR;
    }

	/* Fill in the  icmp header */
	icmph->icmp_type = icmp_type;
	icmph->icmp_code = icmp_code;
	icmph->icmp_checksum = 0;
	ICMP_ECHO_SET_ID(icmph, htons(icmp_id));
	ICMP_ECHO_SET_SEQ(icmph, htons(icmp_seq));

	/* Fill in the icmp data */
	if (len > 0)
		memcpy((void *) (icmph + 1), icmpd, len);

	/* Calculate ICMP checksum with header and data */
	icmph->icmp_checksum =
		icmp_checksum((uint16_t *)icmph, sizeof(struct icmphdr) + len);

#if DBGMSG
	dump_icmp_packet(icmph, saddr, daddr);
#endif
    pkt_len = mbuf->l2_len + mbuf->l3_len + sizeof(struct icmphdr) + len;
	io_send_mbuf(qstack, 0, mbuf, pkt_len);
	return SUCCESS;
}
/*----------------------------------------------------------------------------*/
/**
 * process ICMP echo request
 * @param qstack stack process context
 * @param iph ip header
 * @param len length of ip packet ???
 * @return
 *		return 0 if success, else return ERROR
 */
static int
process_icmp_echo_request(qstack_t qstack, struct iphdr *iph, int len)
{
	int ret = 0;
	struct icmphdr *icmph = (struct icmphdr *)IP_NEXT_PTR(iph);

	/* check correctness of ICMP checkum and send ICMP echo reply */
	if (icmp_checksum((uint16_t *)icmph, len - (iph->ihl << 2)))
		ret = ERROR;
	else
		icmp_output(qstack, iph->daddr, iph->saddr, ICMP_ECHOREPLY, 0,
				ntohs(ICMP_ECHO_GET_ID(icmph)), ntohs(ICMP_ECHO_GET_SEQ(icmph)),
				(uint8_t *) (icmph + 1),
				(uint16_t) (len - (iph->ihl << 2) - sizeof(struct icmphdr)));
	return ret;
}
/******************************************************************************/
/* functions */
/**
 * request ICMP message
 * @param qstack stack process context
 * @param saddr sender ip address
 * @param daddr destination ip address
 * @param icmp_id ICMP id
 * @param icmp_sequence ICMP sequence number
 * @param icmpd ICMP packet data
 * @param len length of ICMP data ???
 * @note send icmp request with given parameters
 */
void
request_icmp(qstack_t qstack, uint32_t saddr, uint32_t daddr,
	uint16_t icmp_id, uint16_t icmp_sequence,
	uint8_t *icmpd, uint16_t len)
{
	/* send icmp request with given parameters */
	icmp_output(qstack, saddr, daddr, ICMP_ECHO, 0, ntohs(icmp_id),
		ntohs(icmp_sequence), icmpd, len);
}
/*----------------------------------------------------------------------------*/
/**
 * process ICMP packet
 * @param qstack stack process context
 * @param ifidx device port index
 * @param cur_ts current time
 * @param mbuf
 * @ip_len length of ip packet
 * @return
 *		return TRUE
 */
int process_icmp_packet(qstack_t qstack, int ifidx, uint32_t cur_ts, mbuf_t mbuf, int ip_len)
//int process_icmp_packet(qstack_t qstack, struct iphdr *iph, int len)
{
	struct iphdr *iph = (struct iphdr*)mbuf_get_ip_ptr(mbuf);
	struct icmphdr *icmph = (struct icmphdr *) IP_NEXT_PTR(iph);
	int i;
	int to_me = FALSE;

	/* process the ICMP messages destined to me */
	for (i = 0 ; i < CONFIG.eths_num; i++) {
		if (iph->daddr == CONFIG.eths[i].ip_addr) {
			to_me = TRUE;
		}
	}

	if (!to_me)
		return TRUE;

	switch (icmph->icmp_type) {
		case ICMP_ECHO:
			process_icmp_echo_request(qstack, iph, ip_len);
			break;

		case ICMP_DEST_UNREACH:
			TRACE_INFO("[INFO] ICMP Destination Unreachable message received\n");
			break;

		case ICMP_TIME_EXCEEDED:
			TRACE_INFO("[INFO] ICMP Time Exceeded message received\n");
			break;

		default:
			TRACE_INFO("[INFO] Unsupported ICMP message type %x received\n", icmph->icmp_type);
			break;
	}

	return TRUE;
}
/*----------------------------------------------------------------------------*/
/**
 * dump ICMP packet
 * @param icmph ICMP header
 * @param saddr sender ip address
 * @param daddr destination ip address
 * @return void
 */
void
dump_icmp_packet(struct icmphdr *icmph, uint32_t saddr, uint32_t daddr)
{
	uint8_t *t;

	fprintf(stderr, "ICMP header: \n");
	fprintf(stderr, "Type: %d, "
		"Code: %d, ID: %d, Sequence: %d\n",
		icmph->icmp_type, icmph->icmp_code,
		ntohs(ICMP_ECHO_GET_ID(icmph)), ntohs(ICMP_ECHO_GET_SEQ(icmph)));

	t = (uint8_t *)&saddr;
	fprintf(stderr, "Sender IP: %u.%u.%u.%u\n",
		t[0], t[1], t[2], t[3]);

	t = (uint8_t *)&daddr;
	fprintf(stderr, "Target IP: %u.%u.%u.%u\n",
		t[0], t[1], t[2], t[3]);
}
/*----------------------------------------------------------------------------*/
#undef IP_NEXT_PTR
/******************************************************************************/
