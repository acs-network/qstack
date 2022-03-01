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
 * @file dpdk_module.h
 * @brief io functions from user-level drivers
 * @author ZZ (zhangzhao@ict.ac.cn)
 * @date 2018.10.25
 * @version 0.1
 * @detail Function list: \n
 *   1. io_init(): init io module\n
 *   2. io_recv_check(): get mbufs from NIC's receive queue to recieve pool\n
 *   3. io_recv_mbuf(): get mbufs from receive pool to user\n
 *   4. io_send_check(): send mbufs from send pool to NIC's send queue\n
 *   5. io_send_mbuf: send mbufs from user to send pool\n
 *   6. io_get_uwmbuf(): get mbuf to be writen by user\n
 *   7. io_get_swmbuf(): get mbuf to be writen by stack\n
 *   8. io_free_mbuf(): free mbufs to mbuf pool\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 * NULL
 */
/******************************************************************************/

#ifndef __QING_DPDK_FUNC
#define __QING_DPDK_FUNC
#include <errno.h> 
#include <rte_common.h> 
#include <rte_ethdev.h> 
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_circular_queue.h>
/* for close */
#include <unistd.h>

#include <rte_ip.h>

#include <pthread.h>
#include <assert.h>



#include "qstack.h"
#include "io_module.h"
#include "n21_queue.h"


/*----------------------------------------------------------------------------*/
/* Essential macros */
#define MAX_RUN_CPUS 				MAX_CORE_NUM
#define MAX_DPC_THREAD				MAX_RUN_CPUS
#define MAX_STACK_THREAD			MAX_STACK_NUM
#define MAX_RX_QUEUE_PER_PORT		MAX_STACK_THREAD
#define MAX_TX_QUEUE_PER_PORT		MAX_STACK_THREAD

#define BUF_SIZE            2048
#define MBUF_SIZE             (BUF_SIZE + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
//#define NB_MBUF                163840
#define MEMPOOL_CACHE_SIZE        256
#define RX_IDLE_TIMEOUT            1    /* in micro-seconds */
#define RX_IDLE_THRESH            64

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH             8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH             8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH             4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R); 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH             36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH            0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH            0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST            256/*256*/
#define MAX_PKT_TX_BURST            64/*MAX 128*/
#define MAX_PKT_RX_BURST            128/*MAX 128*/

#define LOOP_BACK_TEST 0

#define TIME_CHECK 0
/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT    2048
#define RTE_TEST_TX_DESC_DEFAULT    2048

#define MAX_FDIR_RULE_NUM 2048
/******************************************************************************/
//#define RTE_RING_T 1
static uint16_t nb_rxd =         RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd =         RTE_TEST_TX_DESC_DEFAULT;

/*----------------------------------------------------------------------------*/

/* packet memory pools for storing packet bufs */
static struct rte_mempool *rx_pktmbuf_pool[MAX_DPC_THREAD] = {NULL};

static struct rte_mempool *tx_pktmbuf_pool[MAX_DPC_THREAD] = {NULL};

static struct rte_mempool *pktmbuf_alloc_pool[MAX_DPC_THREAD] = {NULL};

static struct rte_mempool *pktmbuf_loopback_pool[MAX_DPC_THREAD] = {NULL};

static struct rte_mempool *pktmbuf_clone_pool[MAX_DPC_THREAD] = {NULL};

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

struct fdir_rules
{
    struct rte_flow *flow;
    rte_spinlock_t sl;   
};

static struct fdir_rules dpdk_fdir_rules[RTE_MAX_ETHPORTS][MAX_FDIR_RULE_NUM];

struct dpdk_private_context 
{
    int cpu;                                      ///< dpc's cpu 
    int check_time;
    uint64_t check_last_time;
    uint64_t check_total_time;
    int tx_send_check;
    struct rte_mempool *rx_pktmbuf_pool;    ///< rx mbuf pool,mbuf from this pool will filing DMA descriptor
    struct rte_mempool *tx_pktmbuf_pool;    ///< tx mbuf pool,stack get mbuf space from it for tx packets

    struct rte_mempool *pktmbuf_clone_pool; ///< some tx mbuf has payload and do not want to be free after send,so we clone it 
    struct rte_mempool *pktmbuf_alloc_pool; ///< only use for user space  get mbuf

    struct rte_mempool *pktmbuf_loopback_pool; ///< only use for user space  get mbuf


    struct rte_mbuf *pkts_rxburst[MAX_PKT_BURST];    ///< dpc wating for stack to receive mbuf
    struct rte_mbuf *pkts_txburst[MAX_PKT_BURST];    ///< waiting for driver to send

	//this part for debug do not use in future 
    int rx_num;
    int tx_num;
//    int alloc_num;
//    int free_num;
//    int rx_free_num;
//    int func_free_num;
//    int dequeue_free_num;
//    int enqueue_free_num;
 	int uwget_num;
 	int uwget_fail_num;
	int swget_num;
	int swget_fail_num;
	int uwfree_num;
	int swfree_num;

    //debug over
	
#ifndef RTE_RING_T
    cirq_t rh_mbufs;   ///<  high level receive queue ,this is circle queue
    cirq_t rl_mbufs;   ///<  low level receive queue ,this is circle queue
    cirq_t th_mbufs;   ///<  high level tx queue ,this is circle queue
    cirq_t tl_mbufs;   ///<  low level tx queue ,this is circle queue


    n21q_t free_queue;  ///< queue for free packet across mutil cpu queue,this is a "n to 1" queue
    n21q_t nofree_queue;  ///< queue for free packet which with payload across mutil cpu,this is a "n to 1" queue
    n21q_t rxfree_queue;   ///< queue for free packet which is a receive mbuf from driver across mutil cpu, this is a "n to 1" queue     

#else
    struct rte_ring *rh_mbufs[RTE_MAX_ETHPORTS];
    struct rte_ring *rl_mbufs[RTE_MAX_ETHPORTS];
    struct rte_ring *tl_mbufs[RTE_MAX_ETHPORTS];
    struct rte_ring *th_mbufs[RTE_MAX_ETHPORTS];
    
    struct rte_ring *free_queue[MAX_RUN_CPUS];
    struct rte_ring *nofree_queue[MAX_RUN_CPUS];
    struct rte_ring *rxfree_queue[MAX_RUN_CPUS];

#endif

} __rte_cache_aligned;

/**
 * stats struct passed on from user space to the driver
 */
struct stats_struct {
    uint64_t tx_bytes;
    uint64_t tx_pkts;
    uint64_t rx_bytes;
    uint64_t rx_pkts;
    uint8_t qid;
    uint8_t dev;
};

/*----------------------------------------------------------------------------*/
/* functions for debug*/
/*----------------------------------------------------------------------------*/

int dpdk_total_recv_num();
int dpdk_total_free_num();
int dpdk_total_uwget_num();
int dpdk_uwget_num(int dpc_id);
int dpdk_uwget_fail_num(int dpc_id);
int dpdk_uwfree_num(int dpc_id);
int dpdk_total_swget_num();
int dpdk_swget_num(int dpc_id);
int dpdk_swget_fail_num(int dpc_id);
int dpdk_swfree_num(int dpc_id);
int dpdk_total_uwget_free();

int dpdk_get_rx_last_time(int dpc_id);
/*----------------------------------------------------------------------------*/
/* static private functions */
/*----------------------------------------------------------------------------*/



/* - Function: static inline uint64_t tim_inter_us 
 * - Description: 
 * - Parameters:*
 * @param start: begin time 
 * @param end: end time
 * - Return:
 *   
 * @retrun
 *        return the length of time
 * - Others:
 *   NULL
 * */
//static inline uint64_t time_inter_us(uint64_t start, uint64_t end);


uint64_t time_inter_us(uint64_t start, uint64_t end);
/* - Function:  static void check_all_ports_link_status
 * - Description: check link state and printf it
 * - Parameters:*
 * @param port_num: in this version port num is 1 
 * @param port_mask: in this version port mask is 0xff
 *   
 * @retrun
 *        
 * - Others:
 *   NULL
 * */

void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);

/*----------------------------------------------------------------------------*/
/* public API functions */
/*----------------------------------------------------------------------------*/


/* - Function: void dpdk_init_handle
 * - Description: init function
 * - Parameters:*
 *
 * @param cpu: handle cpu
 * - Return:
 *   
 * @retrun
 *   NULL
 * */

void dpdk_init_handle(int cpu);

/* - Function: int dpdk_link_devices
 * - Description: link userspace driver
 * - Parameters:*
 *
 * @param ctxt: stack process context
 *   
 * @retrun
 *   1 if success
 *   -1 if fail
 * */
int32_t dpdk_link_devices(struct qstack_context *ctxt);



/* - Function: void dpdk_release_pkt
 * - Description: free packets
 * - Parameters:*
 *
 * @param core_id:source core_id 
 * @param ifidx: port num, only for debug
 * @param pkt_data: target free mbuf array
 *   
 * @retrun
 *   NULL
 * */
//void dpdk_release_pkt(struct qstack_context *ctxt, int ifidx, struct rte_mbuf *pkt_data);

void dpdk_release_pkt(int core_id, int ifidx, struct rte_mbuf *pkt_data);
/* - Function: void dpdk_send_pkt
 * - Description: free packets
 * - Parameters:*
 *
 * @param ctxt: stack process context
 * @param ifidx: port id
 *   
 * @retrun
 *   send mbuf num
 * */
int32_t dpdk_send_pkts(struct qstack_context *ctxt, int ifidx);

/* - Function: void dpdk_get_wpkt
 * - Description: get a pktbuf memroy space for sending it
 * - Parameters:*
 *
 * @param ctxt: stack process context
 * @param ifidx: port id
 * @param pktsize: buf size
 *   
 * @retrun
 *   target mbuf
 * */
uint8_t * dpdk_get_wptr(struct qstack_context *ctxt, int ifidx, uint16_t pktsize);


/* - Function: void dpdk_recv_pkts
 * - Description: get a pktbuf from NIC
 * - Parameters:*
 *
 * @param ctxt: stack process context
 * @param ifidx: port id
 *   
 * @retrun
 *   mbuf num
 * */
int32_t dpdk_recv_pkts(struct qstack_context *ctxt, int ifidx);

/* - Function: void dpdk_get_rpkt
 * - Description: get a pktbuf memroy space for reading it
 * - Parameters:*
 *
 * @param ctxt: stack process context
 * @param ifidx: port id
 * @param index: mbuf index num
 * @param len: return pkt len
 * @param pktsize: buf size
 *   
 * @retrun
 *   target mbuf
 * */
uint8_t * dpdk_get_rptr(struct qstack_context *ctxt, int ifidx, int index, uint16_t *len);


/* - Function: void dpdk_select
 * - Description: do not use it in this version
 * - Parameters:*
 *
 * @param ctxt: stack process context
 *   
 * @retrun
 *   0
 * */
int32_t dpdk_select(struct qstack_context *ctxt);

/* - Function: void dpdk_destroy_handle
 * - Description: free qstack_context *ctxt->driver private data
 * - Parameters:*
 *
 * @param ctxt: stack process context
 *   
 * @retrun
 *   NULL
 * */
void dpdk_destroy_handle(struct qstack_context *ctxt);


/* - Function: void dpdk_load_module
 * - Description: init dpdk and userspace driver
 * - Parameters:
 *
 * @param 
 *    NULL   
 * @retrun
 *  NULL
 * */
void dpdk_load_module(void);

#if 0
/* - Function: void dpdk_soft_filter
 * - Description: put pkt_mbuf to two type of queue
 * - Parameters:
 *
 * @param: pkt_mbuf: target mbuf
 *
 * @retrun
 *  1 if high priority queue
 *  0 if low priority queue
 * */
int dpdk_soft_filter(struct rte_mbuf * pkt_mbuf);
#endif

/* - Function: void dpdk_print_state
 * - Description: printf NIC port state
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: port: target port 
 *
 * @retrun
 *  NULL
 * */
void dpdk_print_state(struct qstack_context *ctxt,int port);


/* - Function: int32_t  dpdk_get_rx_state
 * - Description: get free chunk num in rx queue
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: level: 0 if low priority queue or 1 if high priority queue
 * @param: port: port num index
 * @retrun
 *  free chunk's num in target queue
 * */
int32_t dpdk_get_rx_state(struct qstack_context *ctxt,int level,int port);


/* - Function: int32_t  dpdk_get_tx_state
 * - Description: get free chunk num in tx queue
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: level: 0 if low priority queue or 1 if high priority queue
 * @param: port: port num index
 * @retrun
 *  free chunk's num in target queue
 * */
int32_t dpdk_get_tx_state(struct qstack_context *ctxt,int level,int port);


/* - Function: void  dpdk_check_tx_ring
 * - Description: only check tx ring,send all the packets in tx queue
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @retrun
 *     tx packets num
 * */
int  dpdk_check_tx_ring(struct qstack_context *ctxt,int stack_num);


/* - Function: void  dpdk_check_rx_ring
 * - Description: only check rx ring,send all the packets in rx queue
 *                 need to do some change
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: stack_num:if ctxt is NULL,means polling stack and use stack_num
 * @retrun
 *     rx packets num
 * */

int  dpdk_check_rx_ring(struct qstack_context *ctxt,int stack_num);


/* - Function: void  dpdk_check_device
 * - Description: check rx ring,send all the packets in rx queue
 *                 and also check tx ring
 *                 dpdk_check_device = dpdk_check_rx_ring + dpdk_check_tx_ring
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: stack_num:if ctxt is NULL,means polling stack and use stack_num
 * @retrun
 *     NULL
 * */
void  dpdk_check_device(struct qstack_context *ctxt);


/* - Function: struct rte_mbuf *  dpdk_rx_receive_one
 * - Description: only receive one mbuf to stack
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: level: 0 if low priority queue or 1 if high priority queue
 * @param: port: port num index
 * @retrun
 *     NULL if fail
 *     target mbuf if success
 * */

struct rte_mbuf*  dpdk_rx_receive_one(struct qstack_context *ctxt,int level,int port);


/* - Function: struct rte_mbuf *  dpdk_rx_receive
 * - Description: only receive mbuf to stack
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: level: 0 if low priority queue or 1 if high priority queue
 * @param: port: port num index
 * @param: ptr: target mbuf 
 * @retrun
 *     num of receive packets
 * */
int32_t  dpdk_rx_receive(struct qstack_context *ctxt,int level,int port,struct rte_mbuf **ptr);
    

/* - Function: struct rte_mbuf *  dpdk_tx_send
 * - Description: only send mbuf to driver
 * - Parameters:
 *
 * @param: ctxt: stack process context
 * @param: level: 0 if low priority queue or 1 if high priority queue
 * @param: port: port num index
 * @param: ptr: target mbuf 
 * @retrun
 *     num of send packets
 * */
int32_t   dpdk_tx_send(struct qstack_context *ctxt,int level,int port,struct rte_mbuf **ptr,int len);

/* - Function: struct rte_mbuf *  dpdk_alloc_mbuf
 * - Description: alloc a mbuf space to stack
 * - Parameters:
 *
 * @param: core_id:target core id 
 * @param: ifidex: alloc packet to stack or to user app
 *                    ifidex is 0 means to stack
 *                    ifidex is 1 means to app
 * @retrun
 *     target chunk space
 * */

struct rte_mbuf* dpdk_alloc_mbuf(int core_id, int ifidx);
//struct rte_mbuf* dpdk_alloc_mbuf(struct qstack_context *ctx, int ifidx);

static inline void dpdk_get_start_ave_time(struct dpdk_private_context *dpc)
{
#if TIME_CHECK
	dpc->check_last_time = rte_rdtsc_precise();
#endif
} 


static inline int dpdk_get_end_ave_time(struct dpdk_private_context *dpc)
{

#if TIME_CHECK
	long int timer = 0;
	int count = dpc->check_time;
    dpc->check_time++;
	dpc->check_total_time = dpc->check_total_time +  rte_rdtsc_precise() - dpc->check_last_time;
	dpc->check_last_time = 0;
	if(dpc->check_time == 1000000)
	{
        timer = (dpc->check_total_time * 1000000000) / ((double)rte_get_tsc_hz()* dpc->check_time);
	    printf("xdma time is %ld ns and dpc->check_time is %d , dpc->check_total_time is%ld \n",timer,dpc->check_time,dpc->check_total_time );
	    dpc->check_time = 0;
		dpc->check_total_time = 0;
	}

	return timer;
#endif
} 


#endif  //!__QING_DPDK_FUNC
