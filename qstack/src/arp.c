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
 * @file arp.c
 * @brief structures and functions for ARP
 * @author Guo Ziting (guoziting@ict.ac.cn)
 * @date 2018.8.17
 * @version 1.0
 * @detail Function list: \n
 *   1. init_arp_table(): initialize ARP table \n
 *   2. get_dst_hwaddr(): get destination hardware address \n
 *   3. register_arp_entry(): register ARP entry \n
 *   4. request_arp(): lookup ARP entry \n
 *   5. process_arp_packet(): process ARP packet \n
 *   6. arp_timer(): check the ARP timeout \n
 *   7. print_arp_table(): print ARP table \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.17
 *   	Author: Guo Ziting
 *   	Modification: create
 *   2. Date: 2018.8.20
 *      Author: Guo Ziting
 *      Modification:
 *       1. process_arp_request()
 *       2. process_arp_reply()
 *       3. process_arp_packet()
 *       4. arp_timer()
 *       5. print_arp_table()
 *       6. dump_arp_packet()
 *   3. Date: 2018.8.21
 *   	  Author: Guo Ziting
 *   	  Modification: modify the comments accroding to tools/templates
 *   4. Date: 2018.8.23
 *      Author: Guo Ziting
 *      Modification: remove lock, modifiy process_arp_request()
 */
/******************************************************************************/
 #ifndef TRACE_LEVEL
// 	#define TRACE_LEVEL	TRACELV_DEBUG
 #endif
/*----------------------------------------------------------------------------*/
#include "qstack.h"
#include "arp.h"
#include "eth_out.h"
#include "debug.h"
/******************************************************************************/
/* local macros */
#define ARP_PAD_LEN 18      /* arp pad length to fit 64B packet size */
#define ARP_TIMEOUT_SEC 1   /* 1 second arp timeout */

#define INADDR_LOOPBACK 0x7f000001 /* 127.0.0.1 */

#define HASH(hwaddr) (htonl(hwaddr) & (ARP_TABLE_SIZE - 1))
#define PROXY_HASH ARP_TABLE_SIZE
/******************************************************************************/
/* forward declarations */
//int get_dst_hwaddr(uint32_t dip, enum proxy proxy, char *haddr);
int
register_arp_entry(uint32_t ip, unsigned char *haddr, uint32_t cur_ts);
static int
arp_output(qstack_t qstack, int nif, int opcode, uint32_t dst_ip, 
		unsigned char *dst_haddr);
/******************************************************************************/
/* local data structures */
enum proxy
{
	PROXY_EXACT=0,
	PROXY_ANY,
	PROXY_NONE
};
enum arp_hrd_format
{
    arp_hrd_ethernet = 1
};
/*----------------------------------------------------------------------------*/
enum arp_opcode
{
    arp_op_request = 1,
    arp_op_reply = 2
};
/*----------------------------------------------------------------------------*/
struct arphdr
{
    uint16_t ar_hrd;     /* hardware address format */
    uint16_t ar_pro;     /* protocol address format */
    uint8_t  ar_hln;     /* hardware address length */
    uint8_t  ar_pln;     /* protocol address length */
    uint16_t ar_op;      /* arp opcode              */

    uint8_t  ar_sha[ETH_ALEN];     /* sender hardware address */
    uint32_t ar_sip;               /* sender ip address       */
    uint8_t  ar_tha[ETH_ALEN];     /* target hardware address  */
    uint32_t ar_tip;               /* target ip address       */

    uint8_t pad[ARP_PAD_LEN];
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
struct arp_queue_entry
{
    uint32_t ip;        /* target ip address */
    int nif_out;        /* output interface number */
    uint32_t ts_out;    /* last sent timestamp */

    TAILQ_ENTRY(arp_queue_entry) arp_link;
};
/*----------------------------------------------------------------------------*/
struct arp_manager
{
    TAILQ_HEAD(, arp_queue_entry) list;
    // pthread_mutex_t lock;
};
/*----------------------------------------------------------------------------*/
struct arp_manager g_arpm;
/******************************************************************************/
/* local static functions */
/*----------------------------------------------------------------------------*/
/**
 * arp output
 * @param qstack stack process context
 * @param nif port index
 * @param opcode operate code
 * @param dst_ip destination ip address
 * @param dst_haddr destination hardware address
 * @param target_haddr target hardware address
 * @return
 *    return SUCCESS if success, else return ERROR
 */
static int 
arp_output(qstack_t qstack, int nif, int opcode, uint32_t dst_ip, 
		unsigned char *dst_haddr) 
{
    if (!dst_haddr) {
        return ERROR;
    }

    struct arphdr *arph;
    mbuf_t mbuf;
    int ret;
    int len;

    mbuf = io_get_swmbuf(qstack->stack_id, nif);
    if (!mbuf) {
        TRACE_EXCP("empty mbuf!\n");
        return ERROR;
    }
    ret = eth_hdr_generate(qstack, mbuf, ETH_P_ARP, nif, dst_haddr);
    if (!ret) {
        TRACE_EXCP("Failed to generate ethernet header.\n");
        return ERROR;
    }

    arph = mbuf_get_arp_ptr(mbuf);
    /* Fill arp header */
    arph->ar_hrd = htons(arp_hrd_ethernet);
    arph->ar_pro = htons(ETH_P_IP);
    arph->ar_hln = ETH_ALEN;
    arph->ar_pln = 4;
    arph->ar_op = htons(opcode);

    /* Fill arp body */
	// TODO: fill it
//    int edix = CONFIG.nif_to_eidx[nif];
	int edix = 0;
    arph->ar_sip = CONFIG.eths[edix].ip_addr;
    arph->ar_tip = dst_ip;

    memcpy(arph->ar_sha, CONFIG.eths[edix].haddr, arph->ar_hln);
	memcpy(arph->ar_tha, dst_haddr, arph->ar_hln);

    memset(arph->pad, 0, ARP_PAD_LEN);

#if DBGMSG
    dump_arp_packet(arph);
#endif
    len = mbuf->l2_len + sizeof(struct arphdr);
    io_send_mbuf(qstack, nif, mbuf, len);
    return 0;
}
/*----------------------------------------------------------------------------*/
/**
 * process ARP request
 * @param qstack stack process context
 * @param arph ARP header
 * @param nif port index
 * @param cur_ts current time
 * @return
 *    return 0
 */
static int 
process_arp_request(qstack_t qstack, struct arphdr *arph, int nif, 
		uint32_t cur_ts) 
{

	arp_output(qstack, nif, arp_op_reply, arph->ar_sip, arph->ar_sha);
	
	return SUCCESS;
}
/*----------------------------------------------------------------------------*/
/**
 * process ARP reply
 * @param qstack stack process context
 * @param arph ARP header
 * @param cur_ts current time
 * @return
 *    return 0
 */
static int 
process_arp_reply(qstack_t qstack, struct arphdr *arph, uint32_t cur_ts) 
{
    struct arp_queue_entry *ent;

    /* remove from the arp request queue */
    TAILQ_FOREACH(ent, &g_arpm.list, arp_link) {
        if (ent->ip == arph->ar_sip) {
            TAILQ_REMOVE(&g_arpm.list, ent, arp_link);
            free(ent);
            break;
        }
    }

    return 0;
}
/******************************************************************************/
/* functions */
/**
 * initialize arp protocol
 * @param
 * @return
 *    return 0 if success, else return -1
 */
int 
arp_init() 
{
	int i;
	for (i = 0; i < FULL_ARP_TABLE_SIZE; i++) {
		CONFIG.arp_tables[i] = NULL;
	}
	TAILQ_INIT(&g_arpm.list);

	return SUCCESS;
}

/**
 * get destination hardware address
 *
 * @param dip 			destination ip address
 * @param proxy			whether it's a proxy
 * @param hwaddr[out]	the dest MAC address
 *
 * @return
 *    return 0 if successfully get the dest MAC address;
 *    otherwise return -1
 */
int
get_dst_hwaddr(uint32_t dip, char *haddr) 
{
	char *ret = sarp_lookup(dip);
	if (unlikely(!ret)) {
		TRACE_ERROR("failed to get MAC address from dest ip %u!\n", dip);
	}
	memcpy(haddr, ret, ETH_ALEN);
	return 0;
#if 0
	struct hash_node *entry;
	unsigned char *haddr = NULL;
	unsigned long hash = HASH(dip);

	for (entry = CONFIG.arp_table[hash]; entry != NULL; entry = entry->next) {
		if (entry->ip == dip) {
			break;
		}
	}

	if (!entry && proxy != PROXY_NONE) {
		for (entry = CONFIG.arp_table[PROXY_HASH]; entry != NULL; entry = entry->next) {
			if ((proxy==PROXY_EXACT) ? (entry->ip==dip) : !((entry->ip^dip)&entry->ip_mask))
				break;
		}
	}

	haddr = entry->haddr;
	return haddr;
#endif
}

/**
 * register ARP entry
 * @param ip ip address
 * @param haddr hardware address
 * @return
 *    return 0
 */
int 
register_arp_entry(uint32_t ip, unsigned char *haddr, uint32_t cur_ts) 
{
	unsigned long hash = HASH(ip);
	struct arp_table *entry;

	for (entry = CONFIG.arp_tables[hash]; entry; entry = entry->next) {
		if (entry->ip == ip) {
			break;
		}
	}

	if (entry) {
/*
 * Entry found; update it.
 */
		memcpy(entry->haddr, haddr, ETH_ALEN);
		entry->last_used = cur_ts;
//		TRACE_LOG("Update a arp entry.\n");
	} else {
/*
 * No entry found. Need to add a new entry to the arp table.
 */
		entry = (struct arp_table *)calloc(1, sizeof(struct arp_table));
		if (entry == NULL) {
			TRACE_EXCP("ARP: No memory for new arp entry\n");
			return ERROR;
		}

		entry->ip = ip;
		entry->ip_mask = -1;
		memcpy(entry->haddr, haddr, ETH_ALEN);
		entry->last_used = cur_ts;

		entry->next = CONFIG.arp_tables[hash];
		CONFIG.arp_tables[hash] = entry;

		TRACE_LOG("Learned a new arp entry.\n");
	}

	return SUCCESS;
}

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
request_arp(qstack_t qstack, uint32_t ip, int nif, uint32_t cur_ts) 
{
    struct arp_queue_entry *ent;
    unsigned char haddr[ETH_ALEN];

    // pthread_mutex_lock(&g_arpm.lock);
    /* if the arp request is in progress, return */
    TAILQ_FOREACH(ent, &g_arpm.list, arp_link) {
        if (ent->ip == ip) {
            // pthread_mutex_unlock(&g_arpm.lock);
            return;
        }
    }

    ent = (struct arp_queue_entry *)calloc(1, sizeof(struct arp_queue_entry));
    ent->ip = ip;
    ent->nif_out = nif;
    ent->ts_out = cur_ts;
    TAILQ_INSERT_TAIL(&g_arpm.list, ent, arp_link);
    // pthread_mutex_unlock(&g_arpm.lock);

    /* else, broadcast arp request*/
    memset(haddr, 0xFF, ETH_ALEN);
    arp_output(qstack, nif, arp_op_request, ip, haddr);
}

/**
 * process arp packet, if operate code is request, call process_arp_request()
 *    if operate code is reply, call process_arp_reply()
 * @param qstack stack process context
 * @param ifidx device port index
 * @param cur_ts current time
 * @param mbuf
 * @return
 *    return TRUE if success
 */
int 
process_arp_packet(qstack_t qstack, int ifidx, uint32_t cur_ts, mbuf_t mbuf) 
{

    unsigned char *pkt_data = mbuf_get_buff_ptr(mbuf);
    struct arphdr *arph = (struct arphdr *)(pkt_data + sizeof(struct ethhdr));
    int i, nif;
    int to_me = FALSE;

	/* parameter check */
	if (arph->ar_hrd != htons(arp_hrd_ethernet) ||
		arph->ar_pro != htons(ETH_P_IP) ||
		arph->ar_hln != ETH_ALEN ||
		arph->ar_pln != 4) {

		// free packet
		return ERROR;
	}

	/* check if the target ip is loopback address */
	if (arph->ar_tip == INADDR_LOOPBACK)
	{
		// if is, free packet
		return SUCCESS;
	}

    /* process the arp messages destined to me */
    for (i = 0; i < CONFIG.eths_num; i++) {
        if (arph->ar_tip == CONFIG.eths[i].ip_addr) {
            to_me = TRUE;
		}
    }

#if DBGMSG
    dump_arp_packet(arph);
#endif

    switch (ntohs(arph->ar_op)) {
        case arp_op_request:
			if (to_me) {
				nif = CONFIG.eths[ifidx].ifindex;
				process_arp_request(qstack, arph, nif, cur_ts);
				break;
			} else {
				break;
			}

        case arp_op_reply:
            process_arp_reply(qstack, arph, cur_ts);
            break;

        default:
            break;
    }

	register_arp_entry(arph->ar_sip, arph->ar_sha, cur_ts);

    return SUCCESS;
}

/**
 * check the ARP timeout
 * @param qstack stack process context
 * @param cur_ts current time
 * @return void
 */
/*
void 
arp_timer(qstack_t qstack, uint32_t cur_ts) 
{
  struct arp_queue_entry *ent, *ent_tmp;

  //if the arp request is timed out, retransmit
  // pthread_mutex_lock(&g_arpm.lock);
  TAILQ_FOREACH_SAFE(ent, &g_arpm.list, arp_link, ent_tmp) {
    if (TCP_SEQ_GT(cur_ts, ent->ts_out + SEC_TO_TS(ARP_TIMEOUT_SEC))) {
      struct in_addr ina;
      ina.s_addr = ent->ip;
	  // TODO: do not use %s in trace
//      TRACE_INFO("[CPU%2d] ARP request for %s timed out.\n",
//				   mtcp->ctx->cpu, inet_ntoa(ina));
			TAILQ_REMOVE(&g_arpm.list, ent, arp_link);
			free(ent);
    }
  }
  // pthread_mutex_unlock(&g_arpm.lock);
}
*/

/**
 * print ARP table
 * @param none
 * @return void
 */
void 
print_arp_table() 
{
    int i;
	struct arp_table *entry;

	/* print out process start information */
	TRACE_LOG("ARP Table:\n");
	for (i = 0; i < FULL_ARP_TABLE_SIZE; i++) {
		for (entry = CONFIG.arp_tables[i]; entry != NULL; entry = entry->next) {
				uint8_t *da = (uint8_t *)&entry->ip;

				TRACE_LOG("IP addr: %u.%u.%u.%u, "
							"dst_hwaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
							da[0], da[1], da[2], da[3],
							entry->haddr[0],
							entry->haddr[1],
							entry->haddr[2],
							entry->haddr[3],
							entry->haddr[4],
							entry->haddr[5]);
		}
	}

	TRACE_LOG("----------------------------------------------------------"
			"-----------------------\n");
}

/* - Function: dump_arp_packet
 * - Description: dump arp packet
 * - Parameters:
 *    1. struct arphdr *arph: arp header
 * - Return: none
 * - Others:
 * */
void 
dump_arp_packet(struct arphdr *arph) 
{
    uint8_t *t;

    fprintf(stderr, "ARP header:\n");
    fprintf(stderr, "Hardware type: %d (len: %d), "
			"protocol type: %d (len: %d), opcode: %d\n",
			ntohs(arph->ar_hrd), arph->ar_hln,
			ntohs(arph->ar_pro), arph->ar_pln, ntohs(arph->ar_op));
	t = (uint8_t *)&arph->ar_sip;
	fprintf(stderr, "Sender IP: %u.%u.%u.%u, "
			"haddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
			t[0], t[1], t[2], t[3],
			arph->ar_sha[0], arph->ar_sha[1], arph->ar_sha[2],
			arph->ar_sha[3], arph->ar_sha[4], arph->ar_sha[5]);
	t = (uint8_t *)&arph->ar_tip;
	fprintf(stderr, "Target IP: %u.%u.%u.%u, "
			"haddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
			t[0], t[1], t[2], t[3],
			arph->ar_tha[0], arph->ar_tha[1], arph->ar_tha[2],
			arph->ar_tha[3], arph->ar_tha[4], arph->ar_tha[5]);
}

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
sarp_set(const char *dip, uint8_t mask, const char *haddr)
{
	struct arp_table *entry = &CONFIG.sarp_table[CONFIG.sarp_num++];
	entry->ip = inet_addr(dip);
	entry->ip_mask = htonl(((1<<mask)-1)<<(32-mask));
	sscanf(haddr, "%x:%x:%x:%x:%x:%x", 
			&entry->haddr[0], &entry->haddr[1],
			&entry->haddr[2], &entry->haddr[3],
			&entry->haddr[4], &entry->haddr[5]);
}

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
sarp_lookup(uint32_t dip)
{
	char *ret = NULL;
	int i;
	struct arp_table *entry;
	for (i=0; i<CONFIG.sarp_num; i++) {
		entry = &CONFIG.sarp_table[i];
		if ((entry->ip & entry->ip_mask) == (dip & entry->ip_mask)) {
			ret = entry->haddr;
			break;
		}
	}
	return ret;
}
