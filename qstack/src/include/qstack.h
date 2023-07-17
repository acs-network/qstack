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
 * @file qstack.h 
 * @brief stack context and global variables
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.12
 * @version 0.1
 * @detail Function list: \n
 *   1. get_global_ctx(): get clobal context of qstack system\n
 *   2. qstack_init(): start and init a qstack thread\n
 *   3. qstack_create_app(): start an application thread\n
 *   4. get_core_context(): return the core context with the target core_id\n
 *   5. get_stack_context(): return the stack process context with core_id\n
 *   6. get_app_context(): return the application process context with app_id
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.15 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __QSTACK_H_
#define __QSTACK_H_
/******************************************************************************/
/* global forward declarations */
struct qstack_context;
struct qapp_context;
struct qcore_context;
struct qstack_context_global;
typedef struct qstack_context *qstack_t;
typedef struct qapp_context *qapp_t;
typedef struct qcore_context *qcore_t;
typedef struct qstack_context_global *g_ctx_t;

extern g_ctx_t g_qstack;
extern unsigned char core_map[];

static inline g_ctx_t get_global_ctx();
/******************************************************************************/
#include "universal.h"
#include <dpdk_mempool.h>
#include "runtime_mgt.h"
#include "circular_queue.h"
#include "debug.h"
#include "ps.h"
#include "mbuf.h"

#include "qepoll.h"
#include "socket.h"
#include "tcp_stream.h"
#include "io_module.h"
#include "stream_queue.h"
#include "arp.h"
#include "timestamp.h"
#include "mbuf_queue.h"
#include "qing_hash.h"
#include "hashtable.h"
/******************************************************************************/
/* global macros */
/******************************************************************************/
// forward declaration
struct flow_group_table;
struct mig_queue;
/******************************************************************************/
/* functional data structures */
struct eth_table
{
	char dev_name[128];
	int ifindex;
	int stat_print;
	unsigned char haddr[ETH_ALEN];
	uint32_t netmask;
//	unsigned char dst_haddr[ETH_ALEN];
	uint32_t ip_addr;
};

struct route_table
{
	uint32_t daddr;
	uint32_t mask;
	uint32_t masked;
	int prefix;
	int nif;
};

struct qstack_config
{
    /* socket mode */
	uint8_t socket_mode;

	/* network interface config */
	struct eth_table *eths;
	int *nif_to_eidx; // mapping physic port indexes to that of the configured port-list
	int eths_num;

	/* route config */
	struct route_table *rtable;		// routing table
	struct route_table *gateway;
	int routes;						// # of entries

	/* arp config */
	uint32_t sarp_num;
	struct arp_table sarp_table[STATIC_ARP_NUM];
	struct arp_table *arp_tables[FULL_ARP_TABLE_SIZE];

	int num_cores;		///< num of total cores
	int stack_thread;		///< num of stack cores
	int app_thread;		///< num of application cores
	int num_servers;	///< num of server listening and calling q_accept()
	int num_mem_ch;
	int max_concurrency;
	int core_offset;

	int max_num_buffers;
	int rcvbuf_size;
	int sndbuf_size;

	int tcp_timewait;
	int tcp_timeout;

	int pri;

	/* adding multi-process support */
	uint8_t multi_process;
	uint8_t multi_process_is_master;

#ifdef ENABLE_ONVM
	/* onvm specific args */
	uint16_t onvm_serv;
  	uint16_t onvm_inst;
  	uint16_t onvm_dest;
#endif
};
extern struct qstack_config CONFIG;

/** context for every send device(NIC port) */
struct sender_context
{
	tcp_stream_queue_single control_queue;	///< ready to send control packets
//	tcp_stream_queue_single send_queue;		///< ready to send data packets
	tcp_stream_queue_single ack_queue;		///< ready to send ack-only packets
	uint8_t if_idx;
};
typedef struct sender_context *sender_t;

struct dest_tq_mgt
{
	uint8_t tq_num;
	uint8_t tq[MAX_SERVER_NUM];
	uint8_t rate_end[MAX_SERVER_NUM];
};
/******************************************************************************/
/* context data structures */
/** context for every qstack network stack process instance */
struct qstack_context
{
	uint8_t stack_id;	///< unique id for every stack process instance
	uint8_t cpu;                    ///< private data for cpu id
	uint32_t flow_cnt;
	rtctx_t	rt_ctx;
	qthread_t thread;
	uint32_t cur_ts;

	sender_t sender;			///< context for every send queue
//	struct mig_queue *mig_queue;		///< migration queue

	void *io_private_context;   ///< private data for driver use
//	int queue;					///< ctxt target queue id
	
	tcp_stream_queue send_event_queue;	///< streams need to send data
	tcp_stream_queue send_hevent_queue;	///< streams need to send high-pri data
	tcp_stream_queue close_queue;	///< streams need to close
	tcp_stream_queue connect_queue;	///< streams need to connect
  
#if TCP_TIMER_TIMEWAIT
	TAILQ_HEAD(timewait_head, tcp_stream) timewait_list;	///< fin_wait 
#endif
#if TCP_TIMER_TIMEOUT
	TAILQ_HEAD(timeout_head, tcp_stream) timeout_list;		///< connection
#endif
#if TCP_TIMER_RTO
	#ifndef TCP_SIMPLE_RTO
	struct rto_hashstore* rto_store;	///< pointer to the rto store structure
	#else
	TAILQ_HEAD(rto_head, tcp_stream) rto_list;		///< connection
	#endif
	int rto_list_cnt;			///< current number of streams in rto list
#endif

//	struct message_queue messages;
	mbufq_mempool	mp_mbufq;	///< mempool for fast alloc mbuf_queue
	struct dest_tq_mgt dtq_mgt;

	struct mainloop_timestamp mloop_ts;
	struct req_stage_timestamp req_stage_ts;
	
// statistic
#if STATISTIC_STATE
	#if STATISTIC_STATE_BASIC
	// basic statistic
	volatile uint32_t pkt_in;			///< packets received by stack
	volatile uint32_t pkt_out;			///< packets sent by stack
	volatile uint32_t lreq_recv_num;	///< num of low-requests received
	volatile uint32_t hreq_recv_num;	///< num of low-requests received
	volatile uint32_t resp_num;			///< num of responses get from sndbuf
	volatile uint32_t rcv_backlog_max;	///< max mbuf backlog during this cycle
	volatile uint32_t flow_created;		///< num of flows ctreated
	
	volatile uint32_t lreq_recv_pre;
	volatile uint32_t hreq_recv_pre;
	volatile uint32_t resp_pre;
	volatile uint32_t pkt_in_pre;
	volatile uint32_t pkt_out_pre;
	
	// detailed statistic
	// throughput estimation
	volatile uint64_t byte_in;			///< bytes of packets received by stack
	volatile uint64_t byte_out;			///< bytes of packets sent by stack
	#endif

	#if STATISTIC_STATE_DETAIL
	// throughput estimation
	volatile uint64_t byte_in_pre;
	volatile uint64_t byte_out_pre;
	// event system
	volatile uint32_t read_event_num;	///< num of event raised by request
	volatile uint32_t read_hevent_num;	///< num of high event raised by req
	volatile uint32_t accept_event_num;	///< num of event reised by accept
	volatile uint32_t app_event_num;    ///< num of events added by app
	volatile uint32_t stack_event_num;  ///< num of events added by stack
	volatile uint32_t event_to_add;		///< num of events to be added 
	volatile uint32_t event_added_done;		///< num of events to be added 
	volatile uint32_t queue_full_num;	///< num of event queue full states
	volatile uint32_t queue_end;		///< num of event queue rear
	volatile uint32_t qepoll_err;	    ///< num of event type is 0
	volatile uint32_t wait_appev_num;   ///< num of event wait out for app
	volatile uint32_t wait_stev_num;    ///< num of event wait out for stack
	volatile uint32_t queue_events;		///< num of queue events
	volatile uint32_t queue_start;		///< num of event queue front

	// mbuf recv, send, alloc and free
	volatile uint32_t req_dropped;		///< request mbuf actively dropped
	volatile uint32_t mbuf_dfreed;		///< mbuf derectly freeed by stack
	volatile uint32_t acked_freed;		///< num of freed acked pkts
	volatile uint32_t recv_called_num;	///< num of q_recv() was called
	volatile uint32_t recv_data_num;    ///< num of q_recv() read data len > 0
	volatile uint32_t retrans_rcv_num;	///< num of retrans pakcets received
	volatile uint32_t retrans_snd_num;	///< num of retrans pakcets received
	volatile uint32_t ack_out;			///< num of ack sent out
	volatile uint32_t handle_fdev_num;  ///< num of handle fdevent
	volatile uint32_t conn_accept_num;  ///< num of connction accept
	volatile uint32_t req_add_num;		///< num of requests added to rcvbuf
	volatile uint32_t mbuf_rx_num;		///< num of mbufs from rx_burst
	volatile uint32_t mbuf_tx_num;		///< num of mbufs from tx_burst

	
	// stack state
	volatile uint32_t stream_estb_num;	///< num of stream established by stack
	volatile uint32_t flow_closed;		///< num of flows closed (destroyed)
	volatile uint32_t recv_check_call;	///< num of io_recv_check() called
	volatile uint32_t recv_check_call_zero;	///< num of io_recv_mbuf() called
	volatile uint32_t recv_mbuf_call;	///< num of io_recv_mbuf() called
	volatile uint32_t idle_loop;		///< num of idle loops
	volatile uint32_t wakeup_num;		///< wakeup in co-work mode
	uint32_t check_point_1;
	uint32_t check_point_2;
	#endif // #if STATISTIC_STATE_DETAIL
#endif // #if STATISTIC_STATE
};

/** context for the whole qstack system */
// most variables in it shoud be stable
struct qstack_context_global
{
	qcore_t	core_contexts[MAX_CORE_NUM];
	qapp_t app_contexts[MAX_APP_NUM]; // stores all qapp with app_id as key
	qstack_t stack_contexts[MAX_STACK_NUM];

	// members frequently used (read)
	struct qing_hash * stream_ht[MAX_STACK_NUM];
	socket_table_t socket_table;
	struct runtime_management runtime_mgt;

//	hashtable_t stream_ht[MAX_STACK_NUM];
	struct qing_hash * listener_ht;
//	hashtable_t listener_ht;
	struct tcp_listeners *listeners;
//	struct flow_group_table	*fg_table;		///< flow group table

	qmempool_t mp_listener;		///< mempool for struct tcp_listener
	qmempool_t mp_stream;		///< mempool for struct tcp_stream

	qmempool_t mp_sndbuf;		///< mempool for struct tcp_send_buffer
	pthread_t monitor_thread;
	pthread_t log_thread;
#if STATISTIC_STATE
	volatile uint64_t last_print_ts;	///< timestamp of last print time
	volatile uint64_t add_event_time;   ///< costs of add an event
	
	#if STATISTIC_STATE_DETAIL
	volatile uint32_t rmbuf_get_num[MAX_CORE_NUM];	///< num of mbufs get from q_recv()
	volatile uint32_t uwmbuf_alloced[MAX_CORE_NUM];	
	volatile uint32_t request_freed[MAX_CORE_NUM];	
	volatile uint32_t accepted_num[MAX_CORE_NUM];
	volatile uint32_t highjump_num[MAX_CORE_NUM];
	
	volatile uint32_t recv_called_num[MAX_CORE_NUM];
	volatile uint32_t write_called_num[MAX_CORE_NUM];
	volatile uint32_t close_called_num[MAX_CORE_NUM];
	
	volatile uint32_t high_event_queue[MAX_CORE_NUM];
	volatile uint32_t low_event_queue[MAX_CORE_NUM];
	volatile uint32_t event_add_num[MAX_STACK_NUM][MAX_APP_NUM];	///< num of event added into epoll
#ifdef STATISTIC_FORWARD_BUFF_LEN
	volatile uint32_t forward_buff_len[MAX_CORE_NUM];	///< length of forward buff in iotepserver
#endif

	// temp
//	volatile uint32_t event_add[2][2];
	#endif
#endif
};

/******************************************************************************/
/* function declarations */
/**
 * init the whole qstack system, entrance of user application	
 *
 * @return qapp_t*
 */
qapp_t* 
qstack_init();

/**
 * create an application thread, and pin it to the target core
 *
 * @param tidp		    the pointer of created application thread 
 * @param core_id		the core on which the application is goning to run
 * @app_handle[out]		the handle of created application thread
 * @param app_func		the entry function of application
 * @param args			the args for app_func
 *
 * @return
 * 	return SUCCESS if success; otherwise return FALSE or ERROR
 * @note
 *  input app_handle with NULL if don't need a qapp return
 */
int
qstack_create_app(pthread_t *tidp, int core_id, qapp_t *app_handle, app_func_t app_func, 
		void *args);

/**
 * pthread join thread over all cores
 *
 * return NULL;
 */
void
qstack_thread_join();

/**
 * alloc and init application thread contexts
 *
 * @param core_id		the cpu core the thread should belong to
 *
 * @return
 * 	the alloced application thread context
 */
qapp_t
qapp_init(int core_id);

/**
 * print statistic state of global context and stack contexts
 *
 * @return null
 */
void
print_network_state();
/*----------------------------------------------------------------------------*/
/* global inline functions */
/**
 * get the global context of the whole qstack system
 *
 * @return 
 * 	return the global context
 */
static inline g_ctx_t
get_global_ctx()
{
	return g_qstack;
}

/**
 * get the context of a certain core
 *
 * @param core_id	the id of the target core
 *
 * @return
 * 	the context of the target core
 */
static inline qcore_t
get_core_context(int core_id)
{
	return get_global_ctx()->core_contexts[core_id];
}

/**
 * get the context of a certain stack thread
 *
 * @param stack_id	the id of the target stack thread
 *
 * @return
 * 	the context of the target stack thread
 * @note
 * 	the stack_id is always equal to core_id
 */
static inline qstack_t
get_stack_context(int stack_id)
{
	return get_global_ctx()->stack_contexts[stack_id];
}

/**
 * get the context of a certain application thread
 *
 * @param app_id	the id of the target application thread
 *
 * @return
 * 	the context of the target application thread
 * @note
 * 	the app_id is usually different from core_id, if want to get the 
 * 	application context on the certain core, use get_core_context()->qapp
 */
static inline qapp_t
get_app_context(int app_id)
{
	return get_global_ctx()->app_contexts[app_id];
}

/**
 * get the runtime management handle from global context
 *
 * @return
 * 	the runtime management handle
 * @note
 *  put this function here instead in runtime_mgt.h because of include 
 *  sequence during compiling
 */
static inline rtmgt_t 
get_runtime_mgt()
{
	return &get_global_ctx()->runtime_mgt;
}
/******************************************************************************/
// task queue dest management
static inline uint8_t
get_rand_dest_tq(qstack_t qstack)
{
	struct dest_tq_mgt *tq_mgt = &qstack->dtq_mgt;
	uint8_t roll = ((uint32_t)rand()) % 100;
	int i;
	for (i=0; i<tq_mgt->tq_num; i++) {
		if (roll <= tq_mgt->rate_end[i]) {
			return tq_mgt->tq[i];
		}
	}
}
/******************************************************************************/
// test function called after basic system initialization
int test_func1();
// test function called after rte_eal_init()
int test_func2();
/******************************************************************************/
#define __QSATCK_H_DONE_
#endif //#ifndef __QSTACK_H_
