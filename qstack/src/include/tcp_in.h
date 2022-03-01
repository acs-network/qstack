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
 * @file tcp_in.h 
 * @brief process incoming tcp packets
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12 
 * @version 1.0
 * @detail Function list: \n
 *   1. process_tcp_packet(): process incoming tcp packet\n
 *   2. ParseTCPOptions(): parse tcp options\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.9 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TCP_IN_H_
#define __TCP_IN_H_
/******************************************************************************/
#include "protocol.h"
#include "qstack.h"
/******************************************************************************/
/* function declarations */
/* - Function: process_tcp_packet
 * - Description: process tcp packet
 * - Parameters:
 *   1. qstack_t qstack: stack process context
 *   2. uint32_t cur_ts: timestamp
 *   3. const int ifidx: NIC port
 *   4. mbuf_t mbuf: mbuf
 *   5. ip_len: the length of ip packet
 * - Return:
 *
 * - Others:
 * */
int
process_tcp_packet(qstack_t qstack, uint32_t cur_ts, const int ifidx,  
		mbuf_t mbuf, int ip_len);
/******************************************************************************/
/* inline functions */
void
raise_read_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		int pri);

void
raise_read_event(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts, 
		int pri);

static inline void 
ParseTCPOptions(tcp_stream *cur_stream, 
		uint32_t cur_ts, uint8_t *tcpopt, int len)
{
	int i;
	unsigned int opt, optlen;

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		
		if (opt == TCP_OPT_END) {	// end of option field
			break;
		} else if (opt == TCP_OPT_NOP) {	// no option
			continue;
		} else {

			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > len) {
				break;
			}

			if (opt == TCP_OPT_MSS) {
				cur_stream->sndvar.mss = *(tcpopt + i++) << 8;
				cur_stream->sndvar.mss += *(tcpopt + i++);
				cur_stream->sndvar.eff_mss = cur_stream->sndvar.mss;
#if TCP_OPT_TIMESTAMP_ENABLED
				cur_stream->sndvar.eff_mss -= (TCP_OPT_TIMESTAMP_LEN + 2);
#endif
			} else if (opt == TCP_OPT_WSCALE) {
				cur_stream->sndvar.wscale_peer = *(tcpopt + i++);
			} else if (opt == TCP_OPT_SACK_PERMIT) {
				cur_stream->sack_permit = TRUE;
			} else if (opt == TCP_OPT_TIMESTAMP) {
				cur_stream->saw_timestamp = TRUE;
				cur_stream->rcvvar.ts_recent = ntohl(*(uint32_t *)(tcpopt + i));
				cur_stream->rcvvar.ts_last_ts_upd = cur_ts;
				i += 8;
			} else {
				// not handle
				i += optlen - 2;
			}
		}
	}
}
/******************************************************************************/
#endif //#ifndef __TCP_IN_H_
