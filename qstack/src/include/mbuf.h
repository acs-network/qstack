/**
 * @file mbuf.h
 * @brief struct and functions for mbuf_t
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.8.28
 * @version 1.0
 * @detail Function list: \n
 *   1. mbuf_get_payload_ptr(): get pointer to the start of tcp payload \n
 *   2. mbuf_get_ip_ptr(): get pointer to the start of ip header \n
 *   3. mbuf_get_icmp_ptr(): get pointer to the start of icmp header \n
 *   4. mbuf_get_arp_ptr(): get pointer to the start of arp header \n
 *   5. mbuf_get_tcp_ptr(): get pointer to the start of tcp header \n
 *   6. mbuf_list_init(): init an mbuf_list \n
 *   7, mbuf_list_pop(): get and remove the first mbuf in mbuf_list \n
 *   8. mbuf_list_append(): add an mbuf to the tail of mbuf_list \n
 *   9. mbuf_list_insert(): insert an mbuf after another mbuf in the mbuf_list \n
 *   10. mbuf_list_add_head(): add an mbuf to the head of mbuf_list \n
 *   11. mbuf_list_merge(): append an mbuf_list after another mbuf_list \n
 *   12. mbuf_free(): free all the mbufs in the mbuf_list \n
 *   13. mbuf_prefetch(): prefetch mbuf header and the payload \n
 *   14. mbuf_get_tcp_payload_len(): get the length of tcp payload
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.24
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __MBUF_H_
#define __MBUF_H_
/******************************************************************************/
#include <rte_mbuf.h>
#include "debug.h"
#include "ps.h"
#include "protocol.h"
/******************************************************************************/
// control of debug info filed in mbuf
#define MBUF_ACOUNT		0	// "acount" in mbuf for mbuf allocation and free
#define MBUF_QTS		0	// "q_ts" for whole life delay estimation
#define MBUF_MBUFTS		0	// "mbuf_ts" for single interval estimation
#define MBUF_STREAMID	1	// "stream_id" for mbuf tracing
#define MBUF_DBG_OP		0	// operation trace in mbuf, share 64B with stream_id
/*----------------------------------------------------------------------------*/
#if (MBUF_ACOUNT + MBUF_QTS + MBUF_MBUFTS + \
		(MBUF_STREAMID || MBUF_DBG_OP)) > 1
	#error use at most one field in debug union of rte_mbuf
#endif
/******************************************************************************/
/* global macros */
#define ETHERNET_HEADER_LEN     14  ///< sizeof(struct ethhdr)
#define IP_HEADER_LEN           20  ///< sizeof(struct iphdr)
#define TCP_HEADER_LEN          20  ///< sizeof(struct tcphdr)
#define TOTAL_TCP_HEADER_LEN    54  ///< total header length
#define MBUF_GET_BUFF	((char *)mbuf->buf_addr + mbuf->data_off)
//#define MBUF_GET_BUFF	((char *)mbuf +128 +128 + 64)
/*----------------------------------------------------------------------------*/
/* type of mbuf->mbuf_state */
#define MBUF_STATE_FREE		0	///< free in the mbuf_pool
#define MBUF_STATE_RCVED	1	///< received from driver and processed
#define MBUF_STATE_RBUFFED	2	///< wait in the receive buffer
#define MBUF_STATE_RREAD	3	///< read by application from q_recv()
#define MBUF_STATE_RDONE	4	///< used by application and ready to be freed
#define MBUF_STATE_TALLOC	5	///< allocated from stack to application layer
#define MBUF_STATE_TBUFFED	6	///< pkt with payload, wait in the send buffer
#define MBUF_STATE_TGNRT	7	///< have been generated and wait to be sent
#define MBUF_STATE_SENT		8	///< pkt with payload, sent but not acked
#define MBUF_STATE_LOSS		9	///< lost pkt with payload, not retransed
#define MBUF_STATE_RETRANS	10	///< lost pkt retransed but not acked
#define MBUF_STATE_ACKED	11	///< pkt acked and wait to be freed
/*----------------------------------------------------------------------------*/
/**
 * A macro that points to the tcp payloadin the mbuf.
 *
 * The returned pointer is cast to type t. Before using this function, the 
 * user must ensure that the lengths of l2 to l4 are given correctly.
 *
 * @param m		The packet mbuf.
 * @param t		The type to cast the result into.
 */
#define rte_pktmbuf_payload(m, t)	\
	((t)((char *)(m)->buf_addr + (m)->data_off +	\
	(m)->l2_len + (m)->l3_len + (m)->l4_len))
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
typedef struct rte_mbuf *mbuf_t;
struct mbuf_list
{
	mbuf_t head;
	mbuf_t tail;
};
typedef struct mbuf_list *mbuf_list_t;
/******************************************************************************/
/* function declarations */
static inline void
mbuf_print_detail(mbuf_t mbuf);
/******************************************************************************/
/* inline functions */
static inline void
mbuf_set_tx_offload(mbuf_t mbuf, uint64_t l2_len, uint64_t l3_len, 
		uint64_t l4_len, uint64_t tso_segsz, uint64_t outer_l3_len, 
		uint64_t outer_l2_len)
{
//	mbuf->l2_len = l2_len;
//	mbuf->l3_len = l3_len;
//	mbuf->l4_len = l4_len;
//	mbuf->tso_segsz = tso_segsz; 
//	mbuf->outer_l3_len = outer_l3_len; // not sure
//	mbuf->outer_l2_len = outer_l2_len; // not sure
	mbuf->tx_offload = (outer_l2_len << 49) + (outer_l3_len << 40) +
			(tso_segsz << 24) + (l4_len << 16) + (l3_len << 7) + l2_len;
}
/*----------------------------------------------------------------------------*/
// mbuf functions

/**
 * get tcp payload length from mbuf
 * @param mbuf	target mbuf
 *
 * @return
 * 	return the lenght of TCP payload in mbuf
 *
 * @note
 * 	the mbuf->l4_len must be available
 */
static inline int
mbuf_get_tcp_payload_len(mbuf_t mbuf)
{
	return mbuf->pkt_len - ETHERNET_HEADER_LEN - IP_HEADER_LEN - mbuf->l4_len;
}

/**
 * get packet content from mbuf_t
 * 
 * @param m 	target mbuf
 * @param t 	target type, e.g. (char *) or (struct iphdr*)
 * @param o 	offset from beginning of packet(from Ethernet header)
 * @return 
 * 	typed point to the request position in the packet,
 * 	e.g. payload in (char*) type, or ip header in (struct iphdr*) type
 */
static inline char *
mbuf_get_buff_ptr(mbuf_t mbuf)
{
	if (mbuf) {
		return MBUF_GET_BUFF;
	} else {
		return NULL;
	}
}

/**
 * get pointer to the tcp payload in the mbuf
 *
 * @param mbuf 		target mbuf
 *
 * @return
 * 	pointer (in char* type) to the start of tcp payload
 */
static inline char *
mbuf_get_payload_ptr(mbuf_t mbuf)
{
	if (mbuf && mbuf->l4_len) {
		return MBUF_GET_BUFF + ETHERNET_HEADER_LEN + IP_HEADER_LEN + 
				mbuf->l4_len;
	} else {
		TRACE_EXCP("try to get content from error mbuf\n");
		return NULL;
	}
}

/**
 * get pointer to the ip header in the mbuf
 *
 * @param mbuf 		target mbuf
 *
 * @return
 * 	pointer (in char* type) to the start of ip header
 */
static inline char *
mbuf_get_ip_ptr(mbuf_t mbuf)
{
	if (mbuf) {
		return MBUF_GET_BUFF + ETHERNET_HEADER_LEN;
	} else {
		TRACE_EXCP("try to get content from error mbuf\n");
		return NULL;
	}
}

/**
 * get pointer to the arp header in the mbuf
 *
 * @param mbuf 		target mbuf
 *
 * @return
 * 	pointer (in char* type) to the start of arp header
 */
static inline char *
mbuf_get_arp_ptr(mbuf_t mbuf)
{
	if (mbuf) {
		return MBUF_GET_BUFF + ETHERNET_HEADER_LEN;
	} else {
		TRACE_EXCP("try to get content from error mbuf\n");
		return NULL;
	}
}

/**
 * get pointer to the tcp header in the mbuf
 *
 * @param mbuf 		target mbuf
 *
 * @return
 * 	pointer (in char* type) to the start of tcp header
 */
static inline char *
mbuf_get_tcp_ptr(mbuf_t mbuf)
{
	if (mbuf) {
		return MBUF_GET_BUFF + ETHERNET_HEADER_LEN + IP_HEADER_LEN;
	} else {
		TRACE_EXCP("try to get content from error mbuf\n");
		return NULL;
	}
}

/**
 * get pointer to the icmp header in the mbuf
 *
 * @param mbuf 		target mbuf
 *
 * @return
 * 	pointer (in char* type) to the start of icmp header
 */
static inline char *
mbuf_get_icmp_ptr(mbuf_t mbuf)
{
	if (mbuf) {
		return MBUF_GET_BUFF + ETHERNET_HEADER_LEN + IP_HEADER_LEN;
	} else {
		TRACE_EXCP("try to get content from error mbuf\n");
		return NULL;
	}
}

/**
 * return a free mbuf back to it's mempool
 *
 * @param core_id	current core id the mbuf was freed from
 * @param mbuf		target mbuf
 *
 * @return null
 */
static inline void
mbuf_free(int core_id, mbuf_t mbuf)
{
	if (unlikely(!mbuf)) {
		TRACE_EXIT("try to free an empty mbuf!\n");
	}
	if (mbuf->mbuf_state == MBUF_STATE_FREE) {
		mbuf_print_detail(mbuf);
#if !DRIVER_PRIORITY
		TRACE_EXCP("dupli-free mbuf @ Core %d\n", core_id);
#endif
	}
	mbuf->mbuf_state = MBUF_STATE_FREE;
	io_free_mbuf(core_id, mbuf);
}

/**
 * prefetch mbuf header and the payload
 *
 * @param mbuf	target mbuf
 *
 * @return null
 */
static inline void
mbuf_prefetch(mbuf_t mbuf)
{
	q_prefetch0(mbuf_get_buff_ptr(mbuf));
	q_prefetch0(mbuf->cacheline1);
}

/**
 * print simple information of mbuf
 *
 * @param mbuf		target mbuf
 *
 * @return null
 */
static inline void
mbuf_print_info(mbuf_t mbuf)
{
	TRACE_MBUF("mbuf: seq:%u\t payloadlen:%u\t state:%u\t"
			" priority:%u\t stream:%d\n",
			mbuf->tcp_seq, mbuf->payload_len, mbuf->mbuf_state, 
			mbuf->priority, mbuf->stream_id);
}

/**
 * print full information of mbuf
 *
 * @param mbuf		target mbuf
 *
 * @return null
 */
static inline void
mbuf_print_detail(mbuf_t mbuf)
{
	TRACE_MBUF("mbuf: seq:%u\t payloadlen:%u\t state:%u\t priority:%u\t "
			"stream:%d\t addr:%p\t len:%u\t"
			" l2_len:%u\t l3_len:%u\t l4_len:%u\n",
			mbuf->tcp_seq, mbuf->payload_len, mbuf->mbuf_state, mbuf->priority, 
			mbuf->stream_id, mbuf, mbuf->pkt_len, 
			mbuf->l2_len, mbuf->l3_len, mbuf->l4_len);
}

static inline void
mbuf_print_bytes(mbuf_t mbuf)
{
	int i;
	TRACE_BYTES("%d bytes in mbuf %p: \n", mbuf->pkt_len, mbuf);
	for (i=0; i<mbuf->pkt_len; i++) {
		TRACE_BYTES_X("%2x ", *((uint8_t)(mbuf_get_buff_ptr(mbuf)+i)));
		if ((i+1) % 16 == 0) {
			TRACE_BYTES_X("\n");
		}
	}
}
/******************************************************************************/
// functions for debug info filed
/*----------------------------------------------------------------------------*/
// mbuf operation trace
enum mbuf_operations {
	MBUF_OP_RCV_SALLOC	= 0,
	MBUF_OP_RCV_SGET	= 1,
	MBUF_OP_RCV_SFREE	= 2,
	MBUF_OP_RB_SPUT		= 3,
	MBUF_OP_RB_UGET		= 4,
	MBUF_OP_RCV_UFREE	= 5,
	MBUF_OP_SND_UALLOC	= 6, 
	MBUF_OP_SB_UPUT		= 7, 
	MBUF_OP_SB_SGET		= 8,
	MBUF_OP_SND_SPUT	= 9,
	MBUF_OP_SB_UACK		= 10,
	MBUF_OP_SB_RTR		= 11,
	MBUF_OP_SB_FREE		= 12,
	MBUF_OP_SND_SALLOC	= 13,
	MBUF_OP_CLEAR_FREE  = 14
};

static inline void
mbuf_set_op(mbuf_t mbuf, enum mbuf_operations operation, uint8_t core_id)
{
#if MBUF_DBG_OP
	uint8_t op_p = mbuf->op_p++;
	mbuf->op_trace[op_p].last_operation = operation;
	mbuf->op_trace[op_p].last_core = core_id;
#endif
}
/******************************************************************************/
// mbuf_list functions 
/**
 * init mbuf_list
 *
 * @param list 		target list
 *
 * @return null
 */
static inline void
mbuf_list_init(mbuf_list_t list)
{
	list->head = NULL;
	list->tail = NULL;
}

/**
 * get the mbuf from the head of the list and remove it from the list
 *
 * @param list 		target list
 *
 * @return
 * 	return the mbuf_t if success, or return NULL if the list is empty
 */
static inline mbuf_t
mbuf_list_pop(mbuf_list_t list)
{
	mbuf_t ret;
	ret = list->head;
	if (ret) {
		list->head = ret->buf_next;
		ret->buf_next = NULL;
		if (!list->head) {
			list->tail = NULL;
		}
	}
	return ret;
}

/**
 * add the mbuf to the tail of the list
 *
 * @param list 		target list
 * @param mbuf 		target mbuf
 *
 * @return null
 */
static inline void
mbuf_list_append(mbuf_list_t list, mbuf_t mbuf)
{
	if (!list->tail) {
		list->head = mbuf;
		list->tail = mbuf;
		mbuf->buf_next = NULL;
	}
	else {
		list->tail->buf_next = mbuf;
		mbuf->buf_next = NULL;
		list->tail = mbuf;
	}
}

/**
 * insert a mbuf after another mbuf in the mbuf_list
 *
 * @param list 		target mbuf_list
 * @param b1 		mbuf to be insert after
 * @param b2 		mbuf to be insert
 *
 * @return null
 */
static inline void
mbuf_list_insert(mbuf_list_t list, mbuf_t b1, mbuf_t b2)
{
	b2->buf_next = b1->buf_next;
	b1->buf_next = b2;
	if (!b2->buf_next) {
		list->tail = b2;
	}
}

/**
 * add a mbuf to the head of the list
 *
 * @param list 		target mbuf_list
 * @param mbuf 		target mbuf
 *
 * @return null
 */
static inline void 
mbuf_list_add_head(mbuf_list_t list, mbuf_t mbuf)
{
	mbuf->buf_next = list->head;
	list->head = mbuf;
	if (!mbuf->buf_next) {
		list->tail = mbuf;
	}
}

/**
 * merge mbuf_list l2 after mbuf list l1
 *
 * @param l1 	mbuf_list to be merged in front
 * @param l2 	mbuf_list to be merged at back
 *
 * @return null
 */
static inline void
mbuf_list_merge(mbuf_list_t l1, mbuf_list_t l2)
{ 
	if (!(l1->tail)) {
		l1->head = l2->head;
		l1->tail = l2->tail;
 	}
	else if (l2->head) {
		l1->tail->buf_next = l2->head;
		l1->tail = l2->tail;
 	}
	l2->head = NULL;
	l2->tail = NULL;
}

/**
 * free every mbufs in the mbuf_list
 *
 * @parem qstack	stack context where the free is called
 * @param list 		target mbuf_list
 *
 * @return null
 */
static inline void
mbuf_list_free(int core_id, mbuf_list_t list)
{
	mbuf_t next;
	mbuf_t iter;

	if (!list) {
		return;
	}
	
	iter = list->head;
	while (iter) {
		next = iter->buf_next;
		RTE_MBUF_PREFETCH_TO_FREE(next); // if m != 0 rte_prefetch0(m)
		// TODO: multi-core mode
		mbuf_set_op(iter, MBUF_OP_CLEAR_FREE, core_id);
		mbuf_free(core_id, iter);
		iter = next;
	}
	list->head = NULL;
	list->tail = NULL;
}

/**
 * trace state of mbufs in the mbuf_list
 *
 * @param list 		target mbuf_list
 *
 * @return null
 */
static inline void
mbuf_list_print_info(mbuf_list_t list)
{
#if QDBG_BUFF
	int count = 0;
	mbuf_t mbuf = list->head;

	while (mbuf) {
		count++;
		mbuf_print_info(mbuf);
		mbuf = mbuf->buf_next;
	}
	TRACE_BUFF("mbuf list length:%d\n", count);
#endif
}

static inline void
mbuf_trace_excp(mbuf_t mbuf)
{
	struct ethhdr *ethh = (struct ethhdr *)mbuf_get_buff_ptr(mbuf);
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	int ip_len = ntohs(iph->tot_len);
	uint8_t *payload    = (uint8_t *)tcph + (tcph->doff << 2);
	int payloadlen = ip_len - (payload - (uint8_t *)iph);
	uint32_t seq = ntohl(tcph->seq);
	uint32_t ack_seq = ntohl(tcph->ack_seq);


	TRACE_EXCP("packet %p from unknown stream! "
			"mbuf seq:%u ack_seq:%u payload_len:%u "
			"syn:%d, fin:%d, rst:%d, ack:%d "
//			"saddr: %s, daddr: %s, "
			"hwaddr: %02X:%02X:%02X:%02X:%02X:%02X to "
			"%02X:%02X:%02X:%02X:%02X:%02X "
			"saddr: %d.%d, daddr: %d.%d "
			"sport: %u, dport: %u\n", 
			mbuf
			, seq, ack_seq, payloadlen
			, tcph->syn, tcph->fin, tcph->rst, tcph->ack
//			, inet_ntoa(iph->saddr), inet_ntoa(iph->daddr)
			, ethh->h_source[0], ethh->h_source[1], ethh->h_source[2] 
			, ethh->h_source[3], ethh->h_source[4], ethh->h_source[5] 
			, ethh->h_dest[0], ethh->h_dest[1], ethh->h_dest[2] 
			, ethh->h_dest[3], ethh->h_dest[4], ethh->h_dest[5] 
			, (iph->saddr & 0xff0000)>>16, iph->saddr >>24
			, (iph->daddr & 0xff0000)>>16, iph->daddr >>24
			, ntohs(tcph->source), ntohs(tcph->dest));
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef __MBUF_H_
