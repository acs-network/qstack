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
 * @file eth_in.h 
 * @brief process incoming ethernet packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 1.0
 * @detail Function list: \n
 *   1. process_eth_packet(): process incoming ethernet packets 
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
#ifndef __ETH_IN_H_
#define __ETH_IN_H_
/******************************************************************************/
#include "ip_in.h"
/******************************************************************************/
/* function declarations */
/* - Function: process_eth_packet
 * - Description: process ethernet packet
 * - Parameters:
 *   1. qstack_t qstack: stack process context 
 *   2. const int ifidx: NIC port
 *   3. uint32_t cur_ts: timestamp
 *   4. mbuf_t mbuf: mbuf
 *   5. int len: packet length
 * - Return:
 *		return SUCCESS header if succeed; otherwise return FAILED or ERROR
 * - Others:
 * */
static inline int 
process_eth_packet(qstack_t qstack, const int ifidx, uint32_t cur_ts, 
		mbuf_t mbuf, int len)
{
	unsigned char *pkt_data = mbuf_get_buff_ptr(mbuf);
	struct ethhdr *ethh = (struct ethhdr *)pkt_data;
	u_short ip_proto = ntohs(ethh->h_proto);
	int ret = TRUE;
	mbuf->mbuf_state = MBUF_STATE_RCVED; 
	mbuf->l2_len = sizeof(struct ethhdr);

//	TRACE_CHECKP("process packet! packet len: %d\n", mbuf->pkt_len);
	if (likely(ip_proto == ETH_P_IP)) {
		/* process ipv4 packet */
		ret = process_ipv4_packet(qstack, ifidx, cur_ts, mbuf, len);
	} else if (ip_proto == ETH_P_ARP) {
		process_arp_packet(qstack, ifidx, cur_ts, mbuf);
	} else {
//		TRACE_EXCP("unexcepted protocol type of Ethernet packet: %u\n", 
//				ip_proto);
		ret = FAILED;
	}
	return ret;
}
/******************************************************************************/
#endif //#ifndef __ETH_IN_H_
