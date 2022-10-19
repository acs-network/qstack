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
 * @file ip_out.h
 * @brief functions for sending out ip packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.8.28
 * @version 0.1
 * @detail Function list: \n
 *   1. generate_ip_packet_standalone(): generate ip header and ethernet
 *   	header without tcp stream context \n
 *   2. generate_ip_packet(): generate ip header and ethernet header with tcp
 *   	stream context \n
 *   3. get_output_interface(): get output NIC port according to dest ip addr
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.13
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __IP_OUT_H_
#define __IP_OUT_H_
/******************************************************************************/
#include "eth_out.h"
#include "protocol.h"
/******************************************************************************/
/* function declarations */
/**
 * generate ip packet without stream context
 *
 * @param qstack stack process context
 * @param mbuf target mbuf
 * @param protocol protocol type
 * @param ip_id ip identification
 * @param saddr source ip address
 * @param daddr dest ip address
 * @param payloadlen length of tcp packet(header+payloadlen) or ICMP packet
 *
 * @return
 * 	return SUCCESS if sendout seccussfully;
 * 	return FAILED if the packet don't need to be send, e.g. send queue is full
 * 	return ERROR if something wrong
 *
 * @note mbuf hera should not be NULL, call mbuf_get_tcphdr_ptr or mbuf_get_icmphdr_ptr
 */
int
generate_ip_packet_standalone(qstack_t qstack, mbuf_t mbuf, uint8_t protocol,
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t payloadlen);

/**
 * generate ip packet with tcp context
 * only called by send_tcp_packet()
 *
 * @param qstack stack process context
 * @param stream target tcp stream
 * @param mbuf target mbuf
 * @param tcplen length of tcp packet (header+payloadlen)
 *
 * @return
 * 	return SUCCESS if sendout seccussfully;
 * 	return FAILED if the packet failed to be send, e.g. not found in arp table
 * 	return ERROR if something wrong
 *
 * @note mbuf hera should not be NULL, call mbuf_get_tcphdr_ptr or mbuf_get_icmphdr_ptr
 */
int
generate_ip_packet(qstack_t qstack, tcp_stream_t stream, mbuf_t mbuf,
		uint16_t tcplen);
/******************************************************************************/
/* inline functions */
static inline int
get_output_interface(uint32_t daddr)
{
#ifdef SINGLE_NIC_PORT
	// TODO: multi NIC port select
	return 0;
#else
	int nif = -1;
	int i;
	int prefix = 0;

	/* Longest prefix matching */
	for (i = 0; i < CONFIG.routes; i++) {
		if ((daddr & CONFIG.rtable[i].mask) == CONFIG.rtable[i].masked) {
			if (CONFIG.rtable[i].prefix > prefix) {
				nif = CONFIG.rtable[i].nif;
				prefix = CONFIG.rtable[i].prefix;
			}
		}
	}

	if (nif < 0) {
		uint8_t *da = (uint8_t *)&daddr;
		TRACE_EXCP("No route to %u.%u.%u.%u\n",
				da[0], da[1], da[2], da[3]);
		assert(0);
	}

	return nif;
#endif
}
/******************************************************************************/
#endif //#ifndef __IP_OUT_H_
