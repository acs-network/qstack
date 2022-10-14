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
 * @file ip_in.h 
 * @brief process incoming IP packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 1.0
 * @detail Function list: \n
 *   1. process_ipv4_packet(): process ipv4 packet\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.10 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __IP_IN_H_
#define __IP_IN_H_
/******************************************************************************/
#include "tcp_in.h"
/******************************************************************************/
/* function declarations */
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	asm("  movl (%1), %0\n"
	    "  subl $4, %2\n"
	    "  jbe 2f\n"
	    "  addl 4(%1), %0\n"
	    "  adcl 8(%1), %0\n"
	    "  adcl 12(%1), %0\n"
	    "1: adcl 16(%1), %0\n"
	    "  lea 4(%1), %1\n"
	    "  decl %2\n"
	    "  jne      1b\n"
	    "  adcl $0, %0\n"
	    "  movl %0, %2\n"
	    "  shrl $16, %0\n"
	    "  addw %w2, %w0\n"
	    "  adcl $0, %0\n"
	    "  notl %0\n"
	    "2:"
	    /* Since the input registers which are loaded with iph and ih
	       are modified, we must also specify them as outputs, or gcc
	       will assume they contain their original values. */
	    : "=r" (sum), "=r" (iph), "=r" (ihl)
	    : "1" (iph), "2" (ihl)
	       : "memory");
	return (__sum16)sum;
}
/*----------------------------------------------------------------------------*/
/* - Function: process_ipv4_packet
 * - Description: process ipv4 packet
 * - Parameters:
 *   1. qstack_t qstack: stack process context 
 *   2. const int ifidx: NIC port
 *   3. uint32_t cur_ts: timestamp
 *   4. mbuf_t mbuf: mbuf
 *   5.	int len: packet lenghth
 * - Return:
 *		return SUCCESS header if succeed; otherwise return FAILED or ERROR
 * - Others:
 * */
static inline int 
process_ipv4_packet(qstack_t qstack, const int ifidx, 
		uint32_t cur_ts, mbuf_t mbuf, int len)
{
	struct iphdr* iph = (struct iphdr*)mbuf_get_ip_ptr(mbuf);
	int ip_len = ntohs(iph->tot_len);
	mbuf->l3_len = sizeof(struct iphdr);
	
//	TRACE_CHECKP("process ip packet at stack %d!\n", qstack->stack_id);
	if (unlikely(iph->ihl != IP_HEADER_LEN >> 2)) {
		TRACE_EXIT("unexcepted ip header length!\n");
	}

#if !LOOP_BACK_TEST_MODE
//	if (ip_fast_csum(iph, iph->ihl)) {
//		return ERROR;
//	}
#endif
	
	//TODO: check the ip address
	if (unlikely(iph->version != 0x4)) {
		TRACE_EXCP("invalid ip packet!\n", mbuf);
		return FAILED;
	}
	if (likely(iph->protocol == IPPROTO_TCP)) {
		return process_tcp_packet(qstack, cur_ts, ifidx, mbuf, ip_len);
	}

	return FAILED;// other unknown protocols
}
/******************************************************************************/
/* inline functions */
/******************************************************************************/
#endif //#ifndef __IP_IN_H_

