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
 * @file icmp.h
 * @brief structures and functions for ICMP
 * @author Guo Ziting (guoziting@ict.ac.cn)
 * @date 2018.8.21
 * @version 1.0
 * @detail Function list: \n
 *   1.request_icmp(): send ICMP request with given parameters \n
 *   2.process_icmp_packet(): process ICMP packet \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.21
 *   	Author: Guo Ziting
 *   	Modification: create
 *   2. Date: 2018.8.22
 *   	Author: Guo Ziting
 *   	Modification: add comments
 */
#ifndef __ICMP_H_
#define __ICMP_H_
/******************************************************************************/
#include "universal.h"
/******************************************************************************/
/* global macros */
/* ICMP types */
#define ICMP_ECHOREPLY       0   /* Echo Reply               */
#define ICMP_DEST_UNREACH    3   /* Destination Unreachable  */
#define ICMP_SOURCE_QUENCH   4   /* Source Quench            */
#define ICMP_REDIRECT        5   /* Redirect                 */
#define ICMP_ECHO            8   /* Echo Request             */
#define ICMP_TIME_EXCEEDED   11  /* Time Exceeded            */
#define ICMP_PARAMETERPROB   12  /* Parameter Problem        */
#define ICMP_TIMESTAMP       13  /* Timestamp Request        */
#define ICMP_TIMESTAMPREPLY  14  /* Timestamp Reply          */
#define ICMP_INFO_REQUEST    15  /* Information Request      */
#define ICMP_INFO_REPLY      16  /* Information Reply        */
#define ICMP_ADDRESS         17  /* Address Mask  Request    */
#define ICMP_ADDRESSREPLY    18  /* Address Mask Reply       */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
struct icmphdr {
  uint8_t  icmp_type;
  uint8_t  icmp_code;
  uint16_t icmp_checksum;
  union {
    struct {
      uint16_t icmp_id;
      uint16_t icmp_sequence;
    } echo;                    // ECHO  | ECHOREPLY
    struct {
      uint16_t unused;
      uint16_t nhop_mtu;
    } dest;                    // DEST_UNREACH
  } un;
};
/******************************************************************************/
/* function declarations */
/* getters and setters for ICMP fields */
#define ICMP_ECHO_GET_ID(icmph)            (icmph->un.echo.icmp_id)
#define ICMP_ECHO_GET_SEQ(icmph)           (icmph->un.echo.icmp_sequence)
#define ICMP_DEST_UNREACH_GET_MTU(icmph)   (icmph->un.dest.nhop_mtu)

#define ICMP_ECHO_SET_ID(icmph, id)        (icmph->un.echo.icmp_id = id)
#define ICMP_ECHO_SET_SEQ(icmph, seq)      (icmph->un.echo.icmp_sequence = seq)
/*----------------------------------------------------------------------------*/
void
request_icmp(qstack_t qstack, uint32_t saddr, uint32_t daddr,
    uint16_t icmp_id, uint16_t icmp_seq, uint8_t *icmpd, uint16_t len);
/*----------------------------------------------------------------------------*/
/**
 * process ICMP packet
 * @param qstack stack process context
 * @param iph ip header
 * @param len length of ip data ???
 * @return
 *		return TRUE
 */
//int
//process_icmp_packet(qstack_t qstack, struct iphdr *iph, int len);
int
process_icmp_packet(qstack_t qstack, int ifidx, uint32_t cur_ts, mbuf_t mbuf, int ip_len);
/******************************************************************************/
/* inline functions */
/******************************************************************************/
#endif /*__ICMP_H_ */
