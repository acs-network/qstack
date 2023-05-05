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
 * @file dpdk_module.c
 * @brief io functions from user-level drivers
 * @author ZZ (zhangzhao@ict.ac.cn)
 * @date 2018.7.12
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.12
 *   	Author: zhangzhao
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
//#include "io_module.h"
/******************************************************************************/
#ifndef TRACE_LEVEL
//    #define TRACE_LEVEL    TRACELV_PROC
#endif
/******************************************************************************/
#include "qstack.h"
#include "n21_queue.h"
#include "circular_queue.h"
#include "dpdk_module.h"

/******************************************************************************/
/* user defined priority */
/* add by shenyifan */
int default_driver_pri_filter(mbuf_t mbuf)
{
	    return 0;
}

int(*driver_pri_filter)(mbuf_t mbuf) = default_driver_pri_filter;
/******************************************************************************/


static struct dpdk_private_context *dpc_list[MAX_RUN_CPUS];

static const struct rte_eth_conf port_conf_default = {
         .rxmode = {
        .mq_mode = ETH_MQ_RX_RSS,
        .max_rx_pkt_len = ETHER_MAX_LEN,
        .split_hdr_size = 0,
        .header_split   = 0, /**< Header Split disabled */
        .hw_ip_checksum = 1, /**< IP checksum offload enabled */
        .hw_vlan_filter = 0, /**< VLAN filtering disabled */
        .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
        .hw_strip_crc   = 0, /**< CRC stripped by hardware */
     },

    .rx_adv_conf = {
       .rss_conf = {
/*      default rss key is 
            .rss_key = { 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
                         0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
                         0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
                         0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 
                         0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, },*/
           .rss_key = NULL, 
           .rss_hf =  ETH_RSS_TCP,
       },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
        .offloads = DEV_TX_OFFLOAD_TCP_CKSUM | DEV_TX_OFFLOAD_IPV4_CKSUM,
    },

};

static const struct rte_eth_rxconf rx_conf = {
    .rx_thresh = {
        .pthresh =         RX_PTHRESH, /* RX prefetch threshold reg */
        .hthresh =         RX_HTHRESH, /* RX host threshold reg */
        .wthresh =         RX_WTHRESH, /* RX write-back threshold reg */
    },
    .rx_free_thresh =         32,
};

static const struct rte_eth_txconf tx_conf = {
    .tx_thresh = {
        .pthresh =         TX_PTHRESH, /* TX prefetch threshold reg */
        .hthresh =         TX_HTHRESH, /* TX host threshold reg */
        .wthresh =         TX_WTHRESH, /* TX write-back threshold reg */
    },
    .tx_free_thresh =         0, /* Use PMD default values */
    .tx_rs_thresh =         0, /* Use PMD default values */
    /*
     * As the example won't handle mult-segments and offload cases,
     * set the flag by default.
     */
    .txq_flags =             0x0,
};

struct rte_fdir_conf fdir_conf = {
//    .mode = RTE_FDIR_MODE_NONE,

    .mode = RTE_FDIR_MODE_PERFECT,
    .pballoc = RTE_FDIR_PBALLOC_64K,
    .status = RTE_FDIR_REPORT_STATUS,
    .mask = {
        .vlan_tci_mask = 0x0,
        .ipv4_mask     = {
            .src_ip = 0xFFFFFFFF,
            .dst_ip = 0xFFFFFFFF,
        },
        .ipv6_mask     = {
            .src_ip = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
            .dst_ip = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        },
        .src_port_mask = 0xFFFF,
        .dst_port_mask = 0xFFFF,
        .mac_addr_byte_mask = 0xFF,
        .tunnel_type_mask = 1,
        .tunnel_id_mask = 0xFFFFFFFF,
    },
    .drop_queue = 127,
};

int add_tx_checksum_offloading_flag(struct rte_mbuf *m);
#define APP_RETA_SIZE_MAX     (ETH_RSS_RETA_SIZE_512 / RTE_RETA_GROUP_SIZE)
void
port_rss_reta_info(int  port_id)
{ 
    uint16_t i, idx, shift;
    int ret;
    struct rte_eth_rss_reta_entry64 reta_conf[APP_RETA_SIZE_MAX];
    struct rte_eth_dev_info dev_info;
    memset(reta_conf, 0, sizeof(reta_conf));
    rte_eth_dev_info_get(port_id, &dev_info);
    

    for(idx = 0;idx < dev_info.reta_size; idx++){
       reta_conf[idx / RTE_RETA_GROUP_SIZE].mask = UINT64_MAX;
     }    
    for(idx = 0;idx < dev_info.reta_size; idx++){
       uint32_t reta_id = idx / RTE_RETA_GROUP_SIZE;
       uint32_t reta_pos = idx % RTE_RETA_GROUP_SIZE;
       reta_conf[reta_id].reta[reta_pos] = 1; // forward all packets to core 1
    }
    assert(port_id == 0);
    ret = rte_eth_dev_rss_reta_update(port_id,reta_conf,dev_info.reta_size);   
    if(ret != 0){
        TRACE_INFO("RSS setup error (RETA update failed)\n");
    }else{
        TRACE_INFO("RSS setup error (RETA update success)\n");
     }
}

void 
dpdk_init_handle(int cpu)
{
    struct dpdk_private_context *dpc =NULL;
    int i, j;

    /* create and initialize private I/O module context */
    dpc = calloc(1, sizeof(struct dpdk_private_context));
    memset(dpc,0,sizeof(struct dpdk_private_context));
    dpc->cpu = cpu;
    dpc_list[cpu] = dpc;

    dpc->rx_pktmbuf_pool = rx_pktmbuf_pool[cpu]; ///< this is for recv packets from NIC
    dpc->tx_pktmbuf_pool = tx_pktmbuf_pool[cpu]; ///< this is for send packets to NIC
    dpc->pktmbuf_alloc_pool = pktmbuf_alloc_pool[cpu]; ///<  this is for userspace  alloc 
    dpc->pktmbuf_clone_pool = pktmbuf_clone_pool[cpu]; //< this is for send packets with payload 
    dpc->pktmbuf_loopback_pool = pktmbuf_loopback_pool[cpu]; //< this is loopback test  
    
	dpc->rh_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->rh_mbufs, MAX_FLOW_PSTACK);  
    TRACE_INFO("init high level recevie queue for dpc->cpu %d address %p",dpc->cpu,dpc->rh_mbufs); 
	
    dpc->rl_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->rl_mbufs, MAX_FLOW_PSTACK);  
    TRACE_INFO("init low level receive queue for dpc->cpu %d address %p",dpc->cpu,dpc->rl_mbufs); 

    dpc->th_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->th_mbufs, MAX_FLOW_PSTACK); ///< init high level tx queue
    TRACE_INFO("init high level tx queue for dpc->cpu %d address %p",dpc->cpu,dpc->th_mbufs); 

    dpc->tl_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->tl_mbufs, MAX_FLOW_PSTACK); ///< init low level tx queue
    TRACE_INFO("init low level tx queue for dpc->cpu %d address %p",dpc->cpu,dpc->tl_mbufs); 
    /* **
     *  init  arcoss mutil cpus free queue
     * */


    dpc->free_queue = (n21q_t)calloc(1, sizeof(struct n21_queue));  ///< this is "n to 1" queue
    n21q_init(dpc->free_queue, CONFIG.num_cores, MAX_FLOW_PSTACK / 20);    


    dpc->nofree_queue = (n21q_t)calloc(1, sizeof(struct n21_queue));  ///< this is "n to 1" queue
    n21q_init(dpc->nofree_queue, CONFIG.num_cores, MAX_FLOW_PSTACK / 20);


    dpc->rxfree_queue = (n21q_t)calloc(1, sizeof(struct n21_queue)); ///< this is "n to 1" queue
    n21q_init(dpc->rxfree_queue, CONFIG.num_cores, MAX_FLOW_PSTACK / 20);

}
/*----------------------------------------------------------------------------*/
int
dpdk_link_devices(struct qstack_context *ctxt)
{
    /* do not use this func now */
    return 0;
}
/*----------------------------------------------------------------------------*/


int dpdk_tx_num_thread_info(int coreid)
{
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[coreid];
    return dpc->tx_send_check;
} 

int dpdk_recv_num_thread_info(int coreid)
{
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[coreid];
    return dpc->rx_num;
}


int dpdk_total_recv_num()
{
    int i = 0;
    uint32_t sum = 0;
    struct dpdk_private_context *dpc = NULL;
    for(i = 0; i<MAX_DPC_THREAD;i++){
    	dpc = dpc_list[i];
        sum = sum + dpc->rx_num;
    }
    
    return sum;
}

int dpdk_total_free_num()
{
    int i = 0;
    uint32_t sum = 0;
    struct dpdk_private_context *free_dpc = NULL;
    for(i = 0; i<MAX_DPC_THREAD;i++){
        free_dpc = dpc_list[i];
        sum = sum + free_dpc->rx_free_num;
   
    }
    
    return sum;
}

void
dpdk_release_pkt(int core_id, int ifidx, struct rte_mbuf *pkt_data)
{
    int targ = 0;
    int source = 0;
    int ret = 0;
    struct dpdk_private_context *dpc = NULL;
    struct dpdk_private_context *free_dpc = NULL;

    struct rte_mbuf *m = (struct rte_mbuf*)pkt_data;        
    free_dpc = dpc_list[core_id];
    targ = pkt_data->score;
    dpc = dpc_list[targ];


    if(core_id == pkt_data->score && pkt_data->pool != dpc->rx_pktmbuf_pool){
        m->payload_len = 0;//reset payload len
        if(m->pool == free_dpc->pktmbuf_alloc_pool) {
#if MBUF_STREAMID
			m->stream_id = 0;
#endif
			m->udata64 = 0;
			rte_wmb();
            rte_mbuf_refcnt_set(m, 1);
            dpc->uwfree_num++;
        }
        rte_pktmbuf_free(m);
        return 0;
    }
    else
    {
        source = core_id;
        if(m->pool == dpc->tx_pktmbuf_pool){

            ret = n21q_enqueue(dpc->free_queue,source,m);
            if(ret!=SUCCESS){ 
                TRACE_EXIT("a free n21q enqueue fail and source core id is %d\n",source);
                return -1;
            }

        }else if(m->pool == dpc->rx_pktmbuf_pool){

	#if MBUF_STREAMID
			m->stream_id = 0;
	#endif

#if RX_FREE_NOATOMIC
			/* if RX FREE_NOATOMIC OPEN ,use N21 queue send back mbuf to rx dpc thread */
            /* this can not be used in master-filter mode */
            do {
			ret = n21q_enqueue(dpc->rxfree_queue,source,pkt_data);
            	if(ret!=SUCCESS) {
				/*if n21q queue full,this means stack thread do not have time to release mbuf in n21 queue */
				/*try again */
                TRACE_EXCP("rxfree n21q enqueue fail and source core id is %d\n",source);
            	}
			}while(ret != SUCCESS);
#else
        	rte_pktmbuf_free(m);
#endif
        }else{           
#if MBUF_STREAMID
			m->stream_id = 0;
#endif
			m->udata64 = 0;
            rte_mbuf_refcnt_set(m, 0);
#if DEBUG_MBUF_NOFREE_BUFF 
			/*if try to debug nofree mbuf */
            rte_pktmbuf_free(m);
#else
			ret = n21q_enqueue(dpc->nofree_queue,source,m);
            if(ret!=SUCCESS){
                TRACE_EXIT("nofree n21q enqueue fail and source core id is %d\n",source);
                return -1;
            }
#endif
            dpc->uwfree_num++;
        }
    }
    return 0;

}
/*----------------------------------------------------------------------------*/
void
dpdk_destroy_handle(struct qstack_context *ctxt)
{
    struct dpdk_private_context *dpc = NULL;
    int i = 0;
    int j = 0;

    dpc= dpc_list[ctxt->stack_id];
    /* free it all up */
    free(dpc);
}
/*----------------------------------------------------------------------------*/
void
dpdk_load_module(void)
{
    int portid, lcore_id, ret;
    /*first init mempool space */

    int coreid = 0;
    struct rte_eth_dev_info dev_info;
	TRACE_PROC("rte_mbuf head size is %d \n",sizeof(struct rte_mbuf));
    int i;
    int  num_devices_attached =  rte_eth_dev_count();
    int ring_count = 1;
    TRACE_LOG("start dpdk_load_module\n");
    for (lcore_id = 0; lcore_id < MAX_CORE_NUM; lcore_id++) 
    {
        char rx_name[RTE_MEMPOOL_NAMESIZE];
        char tx_name[RTE_MEMPOOL_NAMESIZE];
        char alloc_name[RTE_MEMPOOL_NAMESIZE];
        char clone_name[RTE_MEMPOOL_NAMESIZE];
        char ring_name[RTE_MEMPOOL_NAMESIZE];
        uint32_t nb_mbuf;
        sprintf(rx_name, "rx_mbuf_pool-%d", lcore_id);

        sprintf(tx_name, "tx_mbuf_pool-%d", lcore_id);
    
        sprintf(alloc_name, "mbuf_alloc_pool-%d", lcore_id);

        sprintf(clone_name, "clone_name_pool-%d", lcore_id);
        
        sprintf(ring_name, "ring_name_pool-%d", lcore_id);
        nb_mbuf = (CONFIG.max_concurrency / CONFIG.stack_thread) >> MEM_SCALE;//CONFIG.max_concurrency / CONFIG.num_cores;//400000;
        while(ring_count < nb_mbuf)
        {
            ring_count = ring_count * 2;
        }
        /* create the mbuf pools */
        if(lcore_id < CONFIG.stack_thread)
        {
            rx_pktmbuf_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(rx_name,
                nb_mbuf, 0, 0,
                RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_mp_mc");
            if(rx_pktmbuf_pool[lcore_id ] != NULL) {
                TRACE_PROC("rx_pktmbuf_pool[%d] success and pool address is 0x%x size is %d\n",lcore_id,rx_pktmbuf_pool[lcore_id],nb_mbuf);
            }
            tx_pktmbuf_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(tx_name,
                nb_mbuf, 32, 0,
                RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");
             if(tx_pktmbuf_pool[lcore_id ] != NULL) {
                TRACE_PROC("tx_pktmbuf_pool[%d] success and pool address is 0x%x size is %d\n",lcore_id,tx_pktmbuf_pool[lcore_id],nb_mbuf);
            }
        }
        else
        {
            rx_pktmbuf_pool[lcore_id] = NULL;
            tx_pktmbuf_pool[lcore_id] = NULL;
        }
        int nb_alloc_num = 0;//MAX_FLOW_NUM/MAX_CORE_NUM;
        nb_alloc_num = (MAX_FLOW_NUM/CONFIG.stack_thread)>>MEM_SCALE;//CONFIG.max_concurrency / CONFIG.num_cores;//400000;
        pktmbuf_alloc_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(alloc_name,
                 nb_alloc_num, 0, 0,
                 RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");

        pktmbuf_clone_pool[lcore_id] = NULL;//rte_pktmbuf_pool_create_by_ops(clone_name,
    }

    /* Second Initialise each port */
    /* only one port in our test */
    num_devices_attached =  rte_eth_dev_count();
    TRACE_INFO("num_devices_attached is %d \n",num_devices_attached);
    num_devices_attached = 1;
    for (i = 0; i < num_devices_attached; ++i) 
    {
        /* get portid form the index of attached devices */
        portid = 0;
        /*in test only one port */
        /* init port */

        fflush(stdout);
        ret = rte_eth_dev_configure(portid, CONFIG.stack_thread, CONFIG.stack_thread, &port_conf_default);
        if (ret < 0)
        {
            rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
                ret, (unsigned) portid);
        }
        else
        {
            TRACE_INFO("rte_eth_dev_configure success \n");
        }

        /* init one RX queue per CPU */
        fflush(stdout);

        rte_eth_dev_info_get(portid, &dev_info);
        TRACE_PROC("=============tx offload mask is 0x%x \n============\n",dev_info.tx_offload_capa);
        TRACE_PROC("DEV_TX_OFFLOAD_VLAN_INSERT is 0x%x \n",DEV_TX_OFFLOAD_VLAN_INSERT);
        TRACE_PROC("DEV_TX_OFFLOAD_TCP_CKSUM is 0x%x \n",DEV_TX_OFFLOAD_TCP_CKSUM);
        TRACE_PROC("DEV_TX_OFFLOAD_IPV4_CKSUM ix 0x%x \n",DEV_TX_OFFLOAD_IPV4_CKSUM);
        int rxqueue_id = 0;
        int txqueue_id = 0;
        int port = 0;
        for (rxqueue_id = 0; rxqueue_id < CONFIG.stack_thread; rxqueue_id++) 
        {
            ret = 0;    
            TRACE_INFO("MAX_STACK_THREAD is  %d int rte_eth_rx_queue_setup mbuf pool is 0x%x \n",MAX_STACK_THREAD,rx_pktmbuf_pool[rxqueue_id]);

            ret = rte_eth_rx_queue_setup(port, rxqueue_id, nb_rxd,rte_eth_dev_socket_id(port), &rx_conf,rx_pktmbuf_pool[rxqueue_id]);
            if (ret < 0)
            {
                rte_exit(EXIT_FAILURE,
                    "rte_eth_rx_queue_setup:err=%d, port=%u, queueid: %d\n",
                    ret, (unsigned) port, rxqueue_id);
            }
            else
            {
                TRACE_PROC("rxlocre_id is %d and rte_eth_rx_queue_setup success \n",rxqueue_id);
            }
        }

        /* init one TX queue on each port per CPU (this is redundant for this app) */
        fflush(stdout);

        struct rte_eth_txconf *txconf;

        for (txqueue_id = 0; txqueue_id < CONFIG.stack_thread; txqueue_id++) {

            txconf = &dev_info.default_txconf;
            txconf->tx_free_thresh = 0;
            txconf->txq_flags = 0;
            ret = rte_eth_tx_queue_setup(portid, txqueue_id, nb_txd,
                                 rte_eth_dev_socket_id(portid), &tx_conf);
            if (ret < 0)
            {
                rte_exit(EXIT_FAILURE,
                 "rte_eth_tx_queue_setup:err=%d, port=%u, queueid: %d\n",
                 ret, (unsigned) portid, txqueue_id);
            }
            else
            {
                TRACE_PROC("txlocre_id is %d and rte_eth_tx_queue_setup success \n",txqueue_id);
            }
        }
        TRACE_PROC("begin to start device !!!\n");
        /* Start device */
        //port_rss_reta_info(portid);

       
        fflush(stdout);
        ret = rte_eth_dev_start(portid);
        if (ret < 0)
        { 

            TRACE_INFO("port %d err and not done: \n",portid);
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
            ret, (unsigned) portid);
        }

		//rte_eth_promiscuous_enable(portid);
		//disable nic promiscuous 
		rte_eth_promiscuous_disable(portid);
        //init fdir rule
		//do not use in stack in this version
        for(i = 0;i < MAX_FDIR_RULE_NUM; i++)
        {
             dpdk_fdir_rules[portid][i].flow = NULL;

             rte_spinlock_init(&dpdk_fdir_rules[portid][i].sl);
        }
        TRACE_PROC("port %d done: \n",portid);
    }
    struct ether_addr addr;
    rte_eth_macaddr_get(portid, &addr);
    TRACE_PROC("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
               " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            portid,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);
            TRACE_PROC("done: \n");
    
	CONFIG.eths[portid].haddr[0] = addr.addr_bytes[0];
	CONFIG.eths[portid].haddr[1] = addr.addr_bytes[1];
	CONFIG.eths[portid].haddr[2] = addr.addr_bytes[2];
	CONFIG.eths[portid].haddr[3] = addr.addr_bytes[3];
	CONFIG.eths[portid].haddr[4] = addr.addr_bytes[4];
	CONFIG.eths[portid].haddr[5] = addr.addr_bytes[5];

	rss_filter_init(CONFIG.stack_thread, 0);
}
/*----------------------------------------------------------------------------*/
static inline int 
dpdk_soft_filter(struct rte_mbuf * pkt_mbuf)
{
#if DRIVER_PRIORITY
	char *payload = mbuf_get_tcp_ptr(pkt_mbuf) + 32;
	pkt_mbuf->priority = (pkt_mbuf->pkt_len>80 && payload[5] == 0x01);
	return pkt_mbuf->priority;
#else
    return 0;
#endif
}
/*----------------------------------------------------------------------------*/
void 
dpdk_print_state(struct qstack_context *ctxt, int port)
{

    ctxt->cpu = ctxt->stack_id;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];

    struct rte_eth_stats stats;
    rte_eth_stats_get(port, &stats);
    TRACE_INFO("NIC miss and drop packets number is %d\n",stats.imissed);
 
}
/*----------------------------------------------------------------------------*/
int 
dpdk_get_rx_state(struct qstack_context *ctxt, int level, int port)
{
    //ctxt->cpu = ctxt->stack_id;
    int num = 0;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];

    if(level == 0)
    {
        num = cirq_count(dpc->rl_mbufs);
    }
    else
    {
        num = cirq_count(dpc->rh_mbufs);
    }
    return num;
}

int 
dpdk_get_tx_state(struct qstack_context *ctxt, int level, int port)
{
    if(ctxt == NULL){
        return -1;
    }
    ctxt->cpu = ctxt->stack_id;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];
    cirq_t tx_mbufs;
    int num;
    if(level == 0)
    {
         //low private queue   
        tx_mbufs = dpc->tl_mbufs;
    }
    else
    {
        //high private queue
        tx_mbufs = dpc->th_mbufs;    
    }
    num = cirq_count(dpc->th_mbufs);
    return num;

}
int 
dpdk_check_tx_ring(struct qstack_context *ctxt,int stack_num) 
{ 
    int stack_id = 0;
    int last_sort = 0;
    if(ctxt != NULL){
        stack_id = ctxt->stack_id;
    }else{
        stack_id = stack_num;
    }
//    int portid = 0;// port;//CONFIG.eths[ifidx].ifindex;  ///<use it in furture, at that time there will be a list of eth port 
    int ret = 0;
    int i =0;
    int len = 0;
    int num = 0;
    int nb_tx = 0;
    int tx_sum = 0;
    int loop_time = 0;
    struct rte_mbuf *mbuf = NULL;
    struct dpdk_private_context *dpc  = dpc_list[stack_id];

    //dpdk_get_start_ave_time(dpc);  /*for driver local test,do not use now */ 
    num = dpc->tx_num;
    while(!cirq_empty(dpc->th_mbufs))
    {
        dpc->pkts_txburst[num] = cirq_get(dpc->th_mbufs);
        if(dpc->pkts_txburst[num] == NULL )
        {
            TRACE_EXIT("hight mbuf dequeue err \n");
            break;
        }
        rs_ts_check(dpc->pkts_txburst[num]);
        if(IF_TX_CHECK){ 
            if(add_tx_checksum_offloading_flag(dpc->pkts_txburst[num]) != 1)
            {
            TRACE_EXIT("some wrong on offloading \n");
            }
        }
        num++;
        dpc->tx_num = num;

        if(num >= MAX_PKT_TX_BURST)
        {
            while(num > 0){
                nb_tx =   rte_eth_tx_burst(0, dpc->cpu, dpc->pkts_txburst, num);
               
				DSTAT_ADD(ctxt->mbuf_tx_num, nb_tx);
                loop_time++;

                tx_sum = tx_sum + nb_tx;

                dpc->tx_send_check = dpc->tx_send_check + nb_tx;
                num = num - nb_tx;
                dpc->tx_num = num;

                if(num != 0 ){
                    for(i = 0; i< num; i++)
                    {            
                        dpc->pkts_txburst[i] = dpc->pkts_txburst[i + nb_tx];
                    }
                }
		if(loop_time >= MAX_TX_CHECK_BATCH)
		{
			return tx_sum;
		}
            }
        }
    }


    while(!cirq_empty(dpc->tl_mbufs))
    {
        dpc->pkts_txburst[num] = cirq_get(dpc->tl_mbufs);//[portid],&dpc->pkts_txburst[num]);
        if(dpc->pkts_txburst[num] == NULL )
        {
            TRACE_MBUF("low mbuf dequeue err \n");
            break;
        }
        rs_ts_check(dpc->pkts_txburst[num]);
        if(IF_TX_CHECK){
            if(add_tx_checksum_offloading_flag(dpc->pkts_txburst[num]) != 1)
            {
            TRACE_EXIT("some wrong on offloading \n");
            }
        }
        num++;
        dpc->tx_num = num;

        if(num >= MAX_PKT_TX_BURST){
            nb_tx = rte_eth_tx_burst(0, dpc->cpu, dpc->pkts_txburst, num);
			DSTAT_ADD(ctxt->mbuf_tx_num, nb_tx);
            tx_sum = tx_sum + nb_tx;
            num = num - nb_tx;
            dpc->tx_num = num;
            if(num != 0 ){
                for(i = 0; i< num; i++){
                    dpc->pkts_txburst[i] = dpc->pkts_txburst[i + nb_tx];
                }
            }
            break;
        }
    }

    if(num != 0)
    {
        nb_tx =   rte_eth_tx_burst(0, dpc->cpu, dpc->pkts_txburst, num);
		DSTAT_ADD(ctxt->mbuf_tx_num, nb_tx);
        dpc->tx_send_check = dpc->tx_send_check + nb_tx;
        tx_sum = tx_sum + nb_tx;
        for(i = 0;i < nb_tx ; i++){
               dpc->pkts_txburst[i] = NULL;
        }
        num = num - nb_tx;
        dpc->tx_num = num;
        if(num != 0 ) {
            for(i = 0; i< num; i++){
                    dpc->pkts_txburst[i] = dpc->pkts_txburst[i + nb_tx];
            }
        }
    }

    //dpdk_get_end_ave_time(dpc);
    return tx_sum;
}

int 
dpdk_check_rx_ring(struct qstack_context *ctxt,int stack_num)
{
    int ret = 0;
    int stack_id = 0;
    /*if ctxt is not null,means it is mutil core check */
    /*else is pooling check */
    if (ctxt != NULL){
        stack_id = ctxt->stack_id;
    }else{
        stack_id = stack_num;
    }  
    struct dpdk_private_context *dpc = dpc_list[stack_id];
    
    dpdk_get_start_ave_time(dpc);
    rte_wmb();

    int portid = 0;// port;//CONFIG.eths[ifidx].ifindex;
    int level = 0;
    int i = 0;
    int j = 0;
    int free_num = 0;
    int total_num  = 0;
    struct rte_mbuf *ptr;
    int flag = 0;

#if RX_FREE_NOATOMIC
    do {
    ptr = n21q_dequeue(dpc->rxfree_queue);
        if(ptr != NULL)
        {
             rte_pktmbuf_free(ptr);
        }
    }while(ptr != NULL);
    ptr = NULL;
#endif

again:
	for( i = 0 ; i<MAX_PKT_RX_BURST ;i++)
	{
		dpc->pkts_rxburst[i] = NULL;
	}
    ret = rte_eth_rx_burst((uint8_t)portid, dpc->cpu,dpc->pkts_rxburst,MAX_PKT_RX_BURST);
	DSTAT_ADD(ctxt->mbuf_rx_num, ret);

    //dpdk_get_end_ave_time(dpc);
    ptr = NULL;
    for( i = 0 ;i< ret ;i++){
        level = dpdk_soft_filter(dpc->pkts_rxburst[i]);
        ptr  = dpc->pkts_rxburst[i];
#if !DRIVER_PRIORITY
        rs_ts_start(&ctxt->req_stage_ts, ptr);
#endif
#if MBUF_STREAMID
		if(ptr->stream_id != 0) {
           	TRACE_EXIT("a new test here and ptr->stream_id is %d \n",ptr->stream_id);
		}
#endif
        if(level == 0)
        {
            if(cirq_add(dpc->rl_mbufs,ptr) != SUCCESS)
            {    
                rte_pktmbuf_free(ptr);
                TRACE_EXIT("dpc->rl_mbufs full and dpc->cpu %d \n",dpc->cpu);
            }
            //think there will be a bug and need to fix later
        }
        if(level == 1)
        {
#if DRIVER_PRIORITY
	        rs_ts_start(&ctxt->req_stage_ts, ptr);
#endif
            if(cirq_add(dpc->rh_mbufs,ptr) != SUCCESS)
            {
                rte_pktmbuf_free(ptr);
                TRACE_EXIT("dpc->rh_mbufs full and rl count is %d \n",cirq_count(dpc->rh_mbufs));
                //TRACE_EXIT("dpc->rh_mbufs full \n");
            }
        }
        dpc->pkts_rxburst[i] = NULL;
    } 
    if(ret != 0)
    {
        goto again;
    }
    rte_rmb();
    dpdk_get_end_ave_time(dpc);
    total_num = cirq_count(dpc->rh_mbufs) + cirq_count(dpc->rl_mbufs);
    return total_num;
}

void
dpdk_check_device(struct qstack_context *ctxt)
{
    dpdk_check_rx_ring(ctxt,0);
    dpdk_check_tx_ring(ctxt,0);
}

struct rte_mbuf*  
dpdk_rx_receive_one(struct qstack_context *ctxt, int level, int port)
{ 

    struct rte_mbuf *tag = NULL;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];
    
    int portid = 0;
    cirq_t rx_mbufs;

#if DRIVER_PRIORITY
    if (level == 1) {
        rx_mbufs = dpc->rh_mbufs;
		tag = cirq_get(rx_mbufs);
    	if (tag != NULL) {
        	tag->score = dpc->cpu;
			return tag;
    	}
	}
#endif
	rx_mbufs = dpc->rl_mbufs;
    tag = NULL;
    tag = cirq_get(rx_mbufs);
    if (tag != NULL) {
       	tag->score = dpc->cpu;
		return tag;
    } else {
		TRACE_EXIT("failed to recieve packet from rx_pool\n");
	}
}


int
dpdk_rx_receive(struct qstack_context *ctxt, int level, int port, struct rte_mbuf **ptr)
{    
    ctxt->cpu = ctxt->stack_id;
    int num = 0;
    int portid = 0;
    struct mbuf_table_q *item = NULL;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];
    //dpc = (struct dpdk_private_context *) ctxt->io_private_context;
    //change it later 
    //do not use it now
    return num;
}
int add_tx_checksum_offloading_flag(struct rte_mbuf *m)
{ 
    struct iphdr *iph;
    struct tcphdr *tcph;
	struct ethhdr *ethh = (struct ethhdr *)rte_pktmbuf_mtod_offset(m, 
			struct ether_hdr *, 0);
	if (unlikely(ntohs(ethh->h_proto)!=ETH_P_IP)) {
		return 1;
	}
    iph = (struct iphdr *)((uint8_t *)ethh + sizeof(struct ether_hdr));
	if (unlikely(iph->protocol != IPPROTO_TCP)) {
		return 1;
	}
	iph->check = 0;

    tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ihl<<2));
	tcph->check = 0;
    m->l2_len = sizeof(struct ether_hdr);
    m->l3_len = (iph->ihl<<2);
    m->l4_len = (tcph->doff<<2);
	
    m->ol_flags = 0;
    m->ol_flags |= PKT_TX_TCP_CKSUM | PKT_TX_IP_CKSUM | PKT_TX_IPV4 ;
    tcph->check = rte_ipv4_phdr_cksum((struct ipv4_hdr *)iph, m->ol_flags);
    return 1;
}

int   
dpdk_tx_send(struct qstack_context *ctxt, int level, int port, struct rte_mbuf **ptr, int num)
{ 

    int ret = 0;
    int head = 0;
    int tail = 0;
    int i = 0;
    struct dpdk_private_context *dpc = dpc_list[ctxt->cpu];;
    struct dpdk_private_context *dpc_src;
    struct rte_mbuf * clone = NULL;
    struct rte_mbuf *mbuf = NULL;
    cirq_t tx_mbufs;
    dpc= dpc_list[ctxt->cpu];
    int portid = port;//CONFIG.eths[ifidx].ifindex;
    for (i = 0; i< num; i++) {

        dpc_src = dpc_list[ptr[i]->score];
        mbuf = ptr[i];

        mbuf->priority = level;
        //item->level = item->mbuf->priority;
        if (mbuf->priority == 0) {
            tx_mbufs = dpc->tl_mbufs;
        } else if (mbuf->priority == 1){
            tx_mbufs = dpc->th_mbufs;
        }
        if (unlikely(cirq_full(tx_mbufs))) {
            TRACE_EXIT("dpc->tx_mbufs %d is full!\n", mbuf->priority);
            break;
        }
        dpc_src = dpc_list[ptr[i]->score];
        if (ptr[i]->pool == dpc_src->tx_pktmbuf_pool || 
                ptr[i]->pool == dpc_src->rx_pktmbuf_pool) {
            mbuf = ptr[i];
        } else {
			
            rte_mbuf_refcnt_set(ptr[i], 2);
            //set mbuf refcnt to 2,so when we free this mbuf in the first time
            //mbuf will not be putted back to mbuf pool,we still can use it
            mbuf = ptr[i];
        }
        if(cirq_add(tx_mbufs,mbuf) != SUCCESS) {
             TRACE_EXIT("enqueue dpc->tx_mbufs %d is fail!\n", mbuf->priority);
		}
        ret = ret + 1;
        rs_ts_add(mbuf->q_ts, REQ_ST_RSPSENT);
    }

    return ret;
}


struct rte_mbuf* 
dpdk_alloc_mbuf(int core_id, int type)
{ 
    struct rte_mbuf *ptr = NULL;
    int portid = 0;
    int i = 0;
	int time = 0;
    struct dpdk_private_context *dpc = NULL;
    struct mbuf_table_q *item = NULL;
    int free_sum = 0;
    dpc = dpc_list[core_id];

    if(dpc->pktmbuf_alloc_pool == NULL) 
    {
        TRACE_EXIT("pktmbuf_alloc_pool is NULL and core_id is %d and type is %d \n",core_id,type);
        return NULL;
    }    

    if(dpc->tx_pktmbuf_pool == NULL && type == 0) 
     {
        TRACE_EXIT("tx_pktmbuf_pool is NULL and core_id is%d and type is %d\n",core_id,type);
        return NULL;
    }   
    if(type == 0) 
    {
 //     TRACE_INFO("this is for driver and stack\n");
        ptr = n21q_dequeue(dpc->free_queue);
       
        if(ptr == NULL)
        {
            ptr = rte_pktmbuf_alloc(dpc->tx_pktmbuf_pool);
        }

        if(ptr == NULL){
            TRACE_INFO("this is for driver and stack fail and dpc is %x dpc->tx_pktmbuf_pool is %x\n",dpc,dpc->tx_pktmbuf_pool);
            return NULL;
        }

        ptr->score = dpc->cpu;
		ptr->payload_len = 0;
		ptr->tcp_seq = 0;
        return ptr;
    } 
    else 
    { 
        ptr = NULL;
		time = 0;
        while(ptr == NULL)
        {
			time++;
            ptr = n21q_dequeue(dpc->nofree_queue);
            if(ptr == NULL)
            {
               ptr = rte_pktmbuf_alloc(dpc->pktmbuf_alloc_pool);
            } 
            if(ptr == NULL) {
                continue;
            }
			if(ptr->udata64 == 2)
			{
				ptr = NULL;
				/* this packet do not free by stack */
				/*try again */
			} 
			
 			if(ptr == NULL)
			{
				continue;
			} else {

				ptr->udata64 = 2;
            	dpc->uwget_num++;
               	ptr->score = core_id;
            	rte_mbuf_refcnt_set(ptr, 2);
			}



        }

           return ptr;
    }
    return NULL;

}
int
dpdk_get_mempool_state(struct qstack_context *ctxt, int id)
{
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];

    int free_num = 0;

    if(dpc == NULL)
    {
        return -1;
    }
    if(id == 0)
    {
        if(dpc->rx_pktmbuf_pool == NULL)
        {
            return  -1;
        }
        else
        {
            free_num = rte_mempool_avail_count(dpc->rx_pktmbuf_pool);
        }
    }     
    if(id == 1)
    {
        if(dpc->pktmbuf_alloc_pool == NULL)
        {
            return  -1;
        }else
        {
            free_num = rte_mempool_avail_count(dpc->pktmbuf_alloc_pool);
        }
    }
    return free_num;
}

int
dpdk_total_uwget_num()
{

    int i = 0;
    int sum = 0;
    struct dpdk_private_context *dpc = NULL;
    for(i = 0; i<CONFIG.num_cores ;i++){
        dpc = dpc_list[i];
        sum = sum + dpc->uwget_num;
    }
    return sum;
}

int
dpdk_uwget_num(int dpc_id)
{
    int sum = 0;
    int i = 0;
    i  =dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    sum = sum + dpc->uwget_num;
    return sum;
}

int
dpdk_uwget_fail_num(int dpc_id)
{
    int sum = 0;
    int i = dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    sum = sum + dpc->uwget_fail_num;
    return sum;
}

int
dpdk_uwfree_num(int dpc_id)
{
    int sum = 0;
    int i = 0;
    i  =dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    sum = sum + dpc->uwfree_num;
    return sum;
}

int
dpdk_total_uwget_free()
{ 
    int sum = 0;
    int i = 0;
    struct dpdk_private_context *dpc = NULL;


    for(i = 0; i<CONFIG.num_cores ;i++){
        dpc = dpc_list[i];
        sum = sum + dpc->uwfree_num;
    }
    return sum;
}

int
dpdk_total_swget_num()
{ 

    int i = 0;
    int sum = 0;
    struct dpdk_private_context *dpc = NULL;
    for(i = 0; i<CONFIG.num_cores ;i++){
        dpc = dpc_list[i];
        sum = sum + dpc->swget_num;
     }
    return sum;
}

int
dpdk_swget_num(int dpc_id)
{

    uint32_t sum = 0;
    int i = 0;
    i  = dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    //printf("i is %d dpc->cpu  is %d and dpc address is 0x%x and dpc->swget_num is %d and free_num is %d \n",i,dpc->cpu,dpc,dpc->swget_num,dpc->free_num);
    return dpc->swget_num;
}

int
dpdk_swget_fail_num(int dpc_id)
{
    int sum = 0;
    int i = dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    return dpc->swget_fail_num;
}

int
dpdk_swfree_num(int dpc_id)
{
    int sum = 0;
    int i = dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
    sum = sum + dpc->swfree_num;
    return sum;
}

int dpdk_get_rx_last_time(int dpc_id)
{
    int i = dpc_id;
    struct dpdk_private_context *dpc = NULL;
    dpc = dpc_list[i];
//    printf("dpc->check_last_time is %ld and dpc->check_time is %d and dpc->check_total_time is%ld \n",dpc->check_last_time,dpc->check_time,dpc->check_total_time );
    int timer = (dpc->check_total_time * 1000000000) / ((double)rte_get_tsc_hz()* dpc->check_time);
    printf("time is %ld ns and dpc->check_time is %d ,cpu hz is %d dpc->check_total_time is%ld \n",timer ,dpc->check_time,rte_get_tsc_hz(),dpc->check_total_time );
    return timer;
}

int
dpdk_update_fdir_function(port_id,ruleID,core_id)
{
    struct fdir_rules *rule;
    uint16_t rx_q = (uint16_t)core_id;
    uint16_t sport = (uint16_t)ruleID; //need to change it later;
    struct rte_flow *ret = NULL;
    ruleID = ruleID%2048;//only for test need to change it later 
    rule = &dpdk_fdir_rules[port_id][ruleID];
    rte_spinlock_lock(&rule->sl);
    if(rule->flow == NULL)
    {
        ret = generate_tcp_flow(port_id, rx_q,
                sport, 0x0eff, //for test only use 11 bit =2048 rules
                0, 0,NULL);
        rule->flow = ret;
       //create a new flow
    }
    else
    {
       //update a new flow
        rte_flow_destroy(port_id,rule->flow,NULL);
        ret = generate_tcp_flow(port_id, rx_q,
                sport, 0x0eff, //for test only use 11 bit =2048 rules
                0, 0,NULL);
        rule->flow = ret;
    }
    rte_spinlock_unlock(&rule->sl);
    if(ret != NULL)
    {
        return 1;
    }
    else
    {
        return -1;
    }
}
/*----------------------------------------------------------------------------*/
/******************************************************************************/
int 
rss_filter_init(int num_stacks,int port_id)
{
    uint16_t idx;
    int ret; 
    struct rte_eth_rss_reta_entry64 reta_conf[APP_RETA_SIZE_MAX];
    struct rte_eth_dev_info dev_info;
    memset(reta_conf, 0, sizeof(reta_conf));
    rte_eth_dev_info_get(port_id, &dev_info);

    for(idx = 0;idx < dev_info.reta_size; idx++){
       reta_conf[idx / RTE_RETA_GROUP_SIZE].mask = UINT64_MAX;
    }    
    for(idx = 0;idx < dev_info.reta_size; idx++){
		uint32_t reta_id = idx / RTE_RETA_GROUP_SIZE;
		uint32_t reta_pos = idx % RTE_RETA_GROUP_SIZE;
        reta_conf[reta_id].reta[reta_pos] = idx % num_stacks;
    }    

    ret = rte_eth_dev_rss_reta_update(port_id,reta_conf,dev_info.reta_size);   
    if(ret != 0){
        TRACE_ERROR("RETA update failed\n");
        return -1;
    }else{
        TRACE_LOG("RETA update success\n");
        return 0;
    }    
}
