/**
 * @file protocol.h
 * @brief protocol structs, const variables, checksum calculate
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.8.28
 * @version 0.1
 * @detail Function list: \n
 *   1. tcp_csum(): calculate tcp checksum\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.9
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2018.8.28 
 *   	Author: Shen Yifan
 *   	Modification: move checksum calculate from tcp_in.h to here
 *   3. Date: 
 *   	Author:
 *   	Modification:
 */
#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_
/******************************************************************************/
#include <linux/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/in.h>

//#include "qstack.h"
/******************************************************************************/
/* global macros */
/* static TCP macros */
#define TCP_FLAG_FIN	0x01	// 0000 0001
#define TCP_FLAG_SYN	0x02	// 0000 0010
#define TCP_FLAG_RST	0x04	// 0000 0100
#define TCP_FLAG_PSH	0x08	// 0000 1000
#define TCP_FLAG_ACK	0x10	// 0001 0000
#define TCP_FLAG_URG	0x20	// 0010 0000
#define TCP_FLAG_SACK	0x40	// 0100 0000
#define TCP_FLAG_WACK	0x80	// 1000 0000

#define TCP_OPT_FLAG_MSS			0x02	// 0000 0010
#define TCP_OPT_FLAG_WSCALE			0x04	// 0000 0100
#define TCP_OPT_FLAG_SACK_PERMIT	0x08	// 0000 1000
#define TCP_OPT_FLAG_SACK			0x10	// 0001 0000
#define TCP_OPT_FLAG_TIMESTAMP		0x20	// 0010 0000	

#define TCP_OPT_MSS_LEN			4
#define TCP_OPT_WSCALE_LEN		3
#define TCP_OPT_SACK_PERMIT_LEN	2
#define TCP_OPT_SACK_LEN		10
#define TCP_OPT_TIMESTAMP_LEN	10
/*----------------------------------------------------------------------------*/
/* configuration macros */
#define TCP_DEFAULT_MSS			1460
#define TCP_DEFAULT_WSCALE		7
#define TCP_INIT_CWND			4		// initial cwnd
#define TCP_INITIAL_WINDOW		14600	// initial receive window size
#define TCP_MAX_RTX				16
#define TCP_MAX_SYN_RETRY		7
#define TCP_MAX_BACKOFF			7
#define TCP_MAX_WINDOW			65535

#define TCP_SEQ_LT(a,b) 		((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)		((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b) 		((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)		((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)	(TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

#define IP_GET_TAIL(a)			((((uint32_t)(ntohl(a)))<<24)>>24)
/*----------------------------------------------------------------------------*/
/* macros functions */
/* convert timeval to timestamp (precision: 1 ms) */
/*----------------------------------------------------------------------------*/
enum tcp_state
{
	TCP_ST_CLOSED		= 0, 
	TCP_ST_LISTEN		= 1, 
	TCP_ST_SYN_SENT		= 2, 
	TCP_ST_SYN_RCVD		= 3,  
	TCP_ST_ESTABLISHED	= 4, 
	TCP_ST_FIN_WAIT_1	= 5, 
	TCP_ST_FIN_WAIT_2	= 6, 
	TCP_ST_CLOSE_WAIT	= 7, 
	TCP_ST_CLOSING		= 8, 
	TCP_ST_LAST_ACK		= 9, 
	TCP_ST_TIME_WAIT	= 10
};
enum tcp_option
{
	TCP_OPT_END			= 0,
	TCP_OPT_NOP			= 1,
	TCP_OPT_MSS			= 2,
	TCP_OPT_WSCALE		= 3,
	TCP_OPT_SACK_PERMIT	= 4, 
	TCP_OPT_SACK		= 5,
	TCP_OPT_TIMESTAMP	= 8
};
enum tcp_close_reason
{
	TCP_NOT_CLOSED		= 0, 
	TCP_ACTIVE_CLOSE	= 1, 
	TCP_PASSIVE_CLOSE	= 2, 
	TCP_CONN_FAIL		= 3, 
	TCP_CONN_LOST		= 4, 
	TCP_RESET			= 5, 
	TCP_NO_MEM			= 6, 
	TCP_NOT_ACCEPTED	= 7, 
	TCP_TIMEDOUT		= 8
};
/******************************************************************************/
/**
 * calculate tcp checksum
 * @param point to buf tcp header
 * @param len length of tcp packet
 * @param saddr source ip adress
 * @param daddr dest ip address
 * @return
 *		return the calculated checksum for the tcp packet
 */
static inline uint16_t
tcp_csum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
	uint32_t sum;
	uint16_t *w;
	int nleft;
	
	sum = 0;
	nleft = len;
	w = buf;
	
	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	
	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);
	
	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(IPPROTO_TCP);
	
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	
	sum = ~sum;
	
	return (uint16_t)sum;
} 

/**
 * translate ip address with mask
 *
 * @param addr		target ip address (in network order)
 * @param mask		mask length
 *
 * @return
 * 	the masked ip address (in network order)
 */
static inline uint32_t
ip_masked(uint32_t addr, uint8_t mask)
{
	uint32_t addr_h = ntohl(addr);
	uint32_t ret = addr_h & ((1<<mask)-1)<<(32-mask);
	return htonl(ret);
}
#endif //#ifndef __PROTOCOL_H_
