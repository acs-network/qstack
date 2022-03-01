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
 * @file ip_out.c
 * @brief output packets for IP layer
 * @author Shen Yifan (shenyifan.ict.ac.cn)
 * @date 2018.8.28
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.28
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#include "ip_out.h"
#include "ip_in.h"
#include "arp.h"
/******************************************************************************/
/* local functions */
/******************************************************************************/
/* functions */
int
generate_ip_packet_standalone(qstack_t qstack, mbuf_t mbuf, uint8_t protocol, 
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t payloadlen)
{
	struct iphdr *iph;
	int nif;
	unsigned char *haddr = "000000";
	int rc = -1;
	int ret;

//	nif = GetOutputInterface(daddr);
	nif = 0;
	if (nif < 0)
		return NULL;

//	haddr = GetDestinationHWaddr(daddr);
	if (!haddr) {
#if 0
		uint8_t *da = (uint8_t *)&daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
				"is not in ARP table!\n",
				da[0], da[1], da[2], da[3]);
#endif
//		RequestARP(mtcp, daddr, nif, mtcp->cur_ts);
		return NULL;
	}
	if (!mbuf) {
		TRACE_EXCP("empty mbuf!\n");
		return ERROR;
	}
	
	ret = eth_hdr_generate(qstack, mbuf, ETH_P_IP, nif, haddr);
	if (!ret) {
		TRACE_EXCP("failed to generate eth_header\n");
		return NULL;
	}
	iph = mbuf_get_ip_ptr(mbuf);

	iph->ihl = IP_HEADER_LEN >> 2;
//	mbuf->l3_len = IP_HEADER_LEN;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + payloadlen);
	iph->id = htons(ip_id);
	iph->frag_off = htons(IP_DF);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = protocol;
	iph->saddr = saddr;
	iph->daddr = daddr;

#if IF_TX_CHECK
	iph->check = 0;
#else
	iph->check = 0;
	iph->check = ip_fast_csum(iph, iph->ihl);
#endif

	return SUCCESS;
}

int
generate_ip_packet(qstack_t qstack, tcp_stream_t stream, mbuf_t mbuf, 
		uint16_t tcplen)
{
	struct iphdr *iph;
	int nif;
//	unsigned char haddr[6];	
	uint8_t *haddr;
	int ret;

	// TODO: multi NIC port select
	if (stream->sndvar.nif_out >= 0) {
		nif = stream->sndvar.nif_out;
	} else {
		nif = get_output_interface(stream->daddr);
		stream->sndvar.nif_out = nif;
	}

	// TODO: get_dest_hwaddr
	//haddr = get_dst_hwaddr(stream->daddr, 0);
	haddr = stream->dhw_addr;
#if 0	// x86-client-wz
	haddr[0] = 0x90;
	haddr[1] = 0xe2;
	haddr[2] = 0xba;
	haddr[3] = 0x17;
	haddr[4] = 0xa6;
	haddr[5] = 0x73;
#endif
#if 0	// x86-client-fx
	haddr[0] = 0x90;
	haddr[1] = 0xe2;
	haddr[2] = 0xba;
	haddr[3] = 0x17;
	haddr[4] = 0xa6;
	haddr[5] = 0x72;
#endif
#if 0	// zhangzhao
	haddr[0] = 0x90;
	haddr[1] = 0xe2;
	haddr[2] = 0xba;
	haddr[3] = 0x13;
	haddr[4] = 0x0b;
	haddr[5] = 0x41;
#endif
#if 0	// wanwenkai eno2
	haddr[0] = 0x90;
	haddr[1] = 0xe2;
	haddr[2] = 0xba;
	haddr[3] = 0x16;
	haddr[4] = 0x1a;
	haddr[5] = 0x6c;
#endif
#if 0	// wanwenkai enp175s0f0
	haddr[0] = 0x90;
	haddr[1] = 0xe2;
	haddr[2] = 0xba;
	haddr[3] = 0xba;
	haddr[4] = 0x56;
	haddr[5] = 0x88;
#endif

	if (!haddr) {
#if 0
		uint8_t *da = (uint8_t *)&stream->daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
				"is not in ARP table!\n",
				da[0], da[1], da[2], da[3]);
#endif
		/* if not found in the arp table, send arp request and return NULL */
		/* tcp will retry sending the packet later */
		// TODO:request arp
//		request_arp(qstack, stream->daddr, stream->sndvar.nif_out, 
//				qstack->cur_ts);
		return ERROR;
	}
	
	if (!mbuf) {
		TRACE_EXCP("empty mbuf!\n");
		ret = 0 / 0;
		return ERROR;
	}
	ret = eth_hdr_generate(qstack, mbuf, ETH_P_IP, stream->sndvar.nif_out, 
			haddr);
	if (!ret) {
		TRACE_EXCP("failed to generate eth_header\n");
		return ret;
	}
	iph = mbuf_get_ip_ptr(mbuf);

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + tcplen);
	iph->id = htons(stream->sndvar.ip_id++);
	iph->frag_off = htons(0x4000);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = stream->saddr;
	iph->daddr = stream->daddr;

#if IF_TX_CHECK
	iph->check = 0;
#else
	iph->check = 0;
	iph->check = ip_fast_csum(iph, iph->ihl);
#endif
	return SUCCESS;
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
