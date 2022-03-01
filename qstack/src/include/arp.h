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
 * @file arp.h
 * @brief structures and functions for ARP
 * @author Guo Ziting (guoziting@ict.ac.cn)
 * @date 2018.8.20
 * @version 1.0
 * @detail Function list: \n
 *   1. init_arp_table(): initialize ARP table \n
 *   2. get_dst_hwaddr(): get destination hardware address \n
 *   3. register_arp_entry(): register arp entry \n
 *   4. request_arp(): lookup ARP entry \n
 *   5. process_arp_packet(): process ARP packet \n
 *   6. arp_timer(): check the ARP timeout \n
 *   7. print_arp_table(): print ARP table \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.20
 *   	  Author: Guo Ziting
 *   	  Modification: create
 *   2. Date: 2018.8.21
 *   	Author: Guo Ziting
 *   	Modification: modify the comments accroding to tools/templates
 *   3. Date: 2018.8.23
 *      Author: Guo Ziting
 *      Modification: add register_arp_entry()
 */
#ifndef __ARP_H_
#define __ARP_H_
/******************************************************************************/
#include "universal.h"
#include "mbuf.h"
#include "ps.h"
#include "qstack.h"
/******************************************************************************/
/* global macros */
#define ARP_TABLE_SIZE 16
#define STATIC_ARP_NUM 16
#define FULL_ARP_TABLE_SIZE (ARP_TABLE_SIZE + 1)

#ifndef ETH_ALEN
	#define ETH_ALEN 6
#endif

/* add macro if it is not defined in sys/queue.h*/
#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)							\
	for ((var) = TAILQ_FIRST((head));									\
		 (var) && ((tvar) = TAILQ_NEXT((var), field), 1);			\
		 (var) = (tvar))
#endif
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
struct arp_table
{
	struct arp_table 		*next;
	uint32_t 				ip;
	uint32_t 				ip_mask;;
	unsigned char 			haddr[ETH_ALEN];
	uint32_t 				last_used;
};
struct hash_node
{
	struct hash_node *next, **pprev;
	uint32_t ip;						/* IP address of entry  */
	uint32_t ip_mask;					/* Netmask				*/
	unsigned char haddr[ETH_ALEN];		/* Hardware address 	*/
	uint32_t last_used;					/* For arp cache aging 	*/

};

struct hash_head
{
	struct hash_node *first;
};
/******************************************************************************/
/* function declarations */
/*----------------------------------------------------------------------------*/
/* operations for hash table */
/* initialize hash table node */
static inline void 
INIT_HASH_NODE(struct hash_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline int 
hash_empty(const struct hash_head *h)
{
	return !h->first;
}

static inline int 
hash_unhashed(const struct hash_node *h)
{
	return !h->pprev;
}

static inline void 
hash_node_del(struct hash_node *n)
{
	struct hash_node *next = n->next;
	struct hash_node **pprev = n->pprev;
	*pprev = next;
	if (next) {
		next->pprev = pprev;
	}
}

static inline void 
hash_add_head(struct hash_node *n, struct hash_head *h)
{
	struct hash_node *first = h->first;
	n->next = first;
	if (first) {
		first->pprev = &n->next;
	}
	h->first = n;
	n->pprev = &h->first;
}

static inline void 
hash_add_after(struct hash_node *n, struct hash_node *next)
{
	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;
	if (next->next) {
		next->next->pprev = &next->next;
	}
}
/*----------------------------------------------------------------------------*/
/**
 * initialize arp protocol
 * @param
 * @return
 *    return 0 if success, else return -1
 */
int
arp_init();
/*----------------------------------------------------------------------------*/
/**
 * get destination hardware address
 *
 * @param dip 			destination ip address
 * @param proxy			whether it's a proxy
 * @param haddr[out]	the dest MAC address
 *
 * @return
 *    return 0 if successfully get the dest MAC address;
 *    otherwise return -1
 */
int 
get_dst_hwaddr(uint32_t dip, char *haddr);
/*----------------------------------------------------------------------------*/
/**
 * register ARP entry
 * @param ip ip address
 * @param haddr hardware address
 * @return
 *    return 0
 */
int
register_arp_entry(uint32_t ip, unsigned char *haddr, uint32_t cur_ts);
/*----------------------------------------------------------------------------*/
/**
 * request ARP
 * @param qstack stack process context
 * @param ip ip address
 * @param nif port index
 * @param cur_ts current time
 * @return void
 */
void
request_arp(qstack_t qstack, uint32_t ip, int nif, uint32_t cur_ts);
/*----------------------------------------------------------------------------*/
/**
 * process arp packet, if operate code is request, call process_arp_request()
 *    if operate code is reply, call process_arp_reply()
 * @param qstack stack process context
 * @param ifidx device port index
 * @param cur_ts current time
 * @param mbuf
 * @param len length of packet
 * @return
 *    return TRUE if success
 */
int 
process_arp_packet(qstack_t qstack, int ifidx, uint32_t cur_ts, mbuf_t mbuf);
/*----------------------------------------------------------------------------*/
/**
 * check the ARP timeout
 * @param qstack stack process context
 * @param cur_ts current time
 * @return void
 */
void
arp_timer(qstack_t qstack, uint32_t cur_ts);
/*----------------------------------------------------------------------------*/
/******************************************************************************/
/**
 * set static arp
 *
 * @dip		dest ip address (in "xxx.xxx.xxx.xxx")
 * @mask	mask length
 * @haddr	dest MAC address (in "xx:xx:xx:xx:xx:xx")
 *
 * @return NULL
 */
void 
sarp_set(const char *dip, uint8_t mask, const char *haddr);

/**
 * look up static arp table for dest MAC address
 * 
 * @param dip	dest ip address (in network order)
 *
 * @return
 * 	return the MAC address if success;
 * 	Otherwise return NULL
 */
char *
sarp_lookup(uint32_t dip);
/******************************************************************************/
/**
 * print ARP table
 * @param none
 * @return void
 */
void
print_arp_table();
/******************************************************************************/
/* inline functions */
/******************************************************************************/
#endif //ifndef__ARP_H_
