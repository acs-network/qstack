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
/*
  * dpdk_module
  *	An implementation of interface between qingyun stack and dpdk driver
  * qingyun TCP/IP stack can recv/send/alloc/release mbuf by using func in this module 
  * 
  * Based on dpdk code.
  *
  * Authors: zhangzhao
  *
*/


/* for io_module_func def'ns */
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
//            .rss_key = { 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, },
           .rss_key = NULL, 
           //.rss_hf =  ETH_RSS_IP | ETH_RSS_UDP | ETH_RSS_TCP | ETH_RSS_SCTP,
           .rss_hf =  ETH_RSS_TCP,
       },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
        .offloads = DEV_TX_OFFLOAD_TCP_CKSUM | DEV_TX_OFFLOAD_IPV4_CKSUM,
    },
#if 0
    .fdir_conf = 
    {
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
    }
#endif
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
    cirq_init(dpc->rh_mbufs, MAX_FLOW_PSTACK);  ///< init high level recevie queue  

    dpc->rl_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->rl_mbufs, MAX_FLOW_PSTACK);  ///< init low level receive queue

    dpc->th_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->th_mbufs, MAX_FLOW_PSTACK); ///< init high level tx queue

    dpc->tl_mbufs = (cirq_t)calloc(1, sizeof(struct circular_queue));
    cirq_init(dpc->tl_mbufs, MAX_FLOW_PSTACK); ///< init low level tx queue


    /* **
     *  init  arcoss mutil cpus free queue
     * */


    dpc->free_queue = (n21q_t)calloc(1, sizeof(struct n21_queue));  ///< this is "n to 1" queue
    n21q_init(dpc->free_queue, CONFIG.num_cores, 200000);    


    dpc->nofree_queue = (n21q_t)calloc(1, sizeof(struct n21_queue));  ///< this is "n to 1" queue
    n21q_init(dpc->nofree_queue, CONFIG.num_cores, 200000);


    dpc->rxfree_queue = (n21q_t)calloc(1, sizeof(struct n21_queue)); ///< this is "n to 1" queue
    n21q_init(dpc->rxfree_queue, CONFIG.num_cores, 300000);

//    dpc->tx_num = 0;
//    dpc->rx_num = 0;
}
/*----------------------------------------------------------------------------*/
int
dpdk_link_devices(struct qstack_context *ctxt)
{
    /* linking takes place during mtcp_init() */

    return 0;
}
/*----------------------------------------------------------------------------*/
int dpdk_total_recv_num()
{
    int i = 0;
    uint32_t sum = 0;
    struct dpdk_private_context *free_dpc = NULL;
    for(i = 0; i<MAX_DPC_THREAD;i++){
        free_dpc = dpc_list[i];
        sum = sum + free_dpc->rx_num;
    }
    //printf("total recv num is: %d \n",sum);
    return sum;
}

int dpdk_total_free_num()
{
    int i = 0;
    uint32_t sum = 0;
    struct dpdk_private_context *free_dpc = NULL;
    for(i = 0; i<MAX_DPC_THREAD;i++){
        free_dpc = dpc_list[i];
    //    sum = sum + free_dpc->rx_free_num;
    //    printf("core %d free num is: %d \n",i,free_dpc->rx_free_num);
    //    printf("core %d func free num is: %d \n",i,free_dpc->func_free_num);
    }
    //printf("total free num is: %d \n",sum);
    return sum;
}

//dpdk_release_pkt(struct qstack_context *ctxt,int ifidx,struct rte_mbuf *pkt_data)
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
    //    struct rte_mbuf *m = (struct rte_mbuf*)pkt_data;        
        m->payload_len = 0;//reset payload len
        if(m->pool == free_dpc->pktmbuf_alloc_pool) {
       		//_mm_sfence();
#if MBUF_STREAMID
//			m->stream_id = 0;
#endif
			m->udata64 = 0;

			//m->alloc_t++;
			//m->free_t++;
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
#if 0
            ret = n21q_enqueue(dpc->rxfree_queue,source,pkt_data);
            if(ret!=SUCCESS) {
                TRACE_EXIT("rxfree n21q enqueue fail and source core id is %d\n",source);
                return -1;
            }
#else
	#if MBUF_STREAMID
//			m->stream_id = 0;
	#endif
        	rte_pktmbuf_free(m);
        	//TRACE_EXIT("rx add to cir free forward enqueue fail \n");
    	//    rte_pktmbuf_free_forward(m,core_id);
#endif
        }else{
            
            rte_mbuf_refcnt_set(m, 1);
#if MBUF_STREAMID
//			m->stream_id = 0;
#endif
			m->udata64 = 0;
        //   rte_pktmbuf_free(m);
			ret = n21q_enqueue(dpc->nofree_queue,source,m);
            if(ret!=SUCCESS){
                TRACE_EXIT("nofree n21q enqueue fail and source core id is %d\n",source);
                return -1;
            }

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
  //      if(MAX_FLOW_NUM < 300000)
  //      {
        nb_mbuf = MAX_FLOW_PSTACK >> MEM_SCALE;//CONFIG.max_concurrency / CONFIG.num_cores;//400000;
  //      }
  //      else
  //      {
 //	       nb_mbuf = 300000/MAX_STACK_NUM;
  //      }
        while(ring_count < nb_mbuf)
        {
            ring_count = ring_count * 2;
        }
        /* create the mbuf pools */
        if(lcore_id < CONFIG.num_stacks)
        {
            rx_pktmbuf_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(rx_name,
                nb_mbuf, 0, 0,
                RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_mp_mc");
 //               RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");
            if(rx_pktmbuf_pool[lcore_id] == NULL)
            {
                    rte_exit(EXIT_FAILURE, "Cannot init rx_pktmbuf mbuf pool and locrea is %d nb_mbuf is %d\n",lcore_id,nb_mbuf/10);
            }

            rx_pktmbuf_pool[lcore_id]->core_id = lcore_id;
            rx_pktmbuf_pool[lcore_id]->private_type = 1;
            rx_pktmbuf_pool[lcore_id]->ring_num = MAX_CORE_NUM;
            for(i = 0; i< MAX_CORE_NUM ;i++)
            {
                sprintf(ring_name, "ring_name_pl-%d-%d", lcore_id ,i);
                rx_pktmbuf_pool[lcore_id]->local_ring[i] = calloc(1, sizeof(struct rte_circular_queue));
//rte_ring_create(ring_name, ring_count, lcore_id, RING_F_SP_ENQ|RING_F_SC_DEQ);
                if(rx_pktmbuf_pool[lcore_id]->local_ring[i] == NULL)
                {
                    rte_exit(EXIT_FAILURE, "Cannot init rx_pktmbuf mbuf pool's rte ring and locre is %d \n",
                    lcore_id);
                }
                else
                {
                    rte_cirq_init(rx_pktmbuf_pool[lcore_id]->local_ring[i],ring_count);
                }
            }
            //we need to add rte ring to mempool  
            //and set private_type to 1

            tx_pktmbuf_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(tx_name,
                nb_mbuf, 32, 0,
                RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");

        
            if (rx_pktmbuf_pool[lcore_id] == NULL)
            {
                rte_exit(EXIT_FAILURE, "Cannot init rx_pktmbuf mbuf pool rxlcore_id is %d, errno: %d\n",
                lcore_id,rte_errno);
            }
            else
            {
                TRACE_MEMORY("alloc rx_pktmbuf_pool[%d] success and pool address is 0x%x \n",lcore_id,rx_pktmbuf_pool[lcore_id]);
            }


            if (tx_pktmbuf_pool[lcore_id] == NULL)
            {
            rte_exit(EXIT_FAILURE, "Cannot init tx_pktmbuf_pool mbuf pool txlcore_id is %d, errno: %d\n",lcore_id,
                 rte_errno);
            }
            else
            {
                TRACE_MEMORY("alloc tx_pktmbuf_pool[%d] success and pool address is 0x%x \n",lcore_id,tx_pktmbuf_pool[lcore_id]);
            }
        }
        else
        {
            rx_pktmbuf_pool[lcore_id] = NULL;
            tx_pktmbuf_pool[lcore_id] = NULL;
        }
        int nb_alloc_num = 0;//MAX_FLOW_NUM/MAX_CORE_NUM;

//        if(MAX_FLOW_NUM < 300000)
//        {
        nb_alloc_num = (MAX_FLOW_NUM/CONFIG.num_stacks)>>MEM_SCALE;//CONFIG.max_concurrency / CONFIG.num_cores;//400000;
//        }
//        else
//        {
//            nb_alloc_num = 300000/MAX_STACK_NUM;
//        }
        pktmbuf_alloc_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(alloc_name,
                 nb_alloc_num, 0, 0,
                 RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");

        pktmbuf_alloc_pool[lcore_id]->private_type = 2;

                 //RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_mp_mc");
        if (pktmbuf_alloc_pool[lcore_id] == NULL){
            rte_exit(EXIT_FAILURE, "Cannot init alloc mbuf pool and lcore_id is %d, errno: %d\n",lcore_id,
                 rte_errno);
        }
        else
        {
                TRACE_MEMORY("alloc pktmbuf_pool[%d] success and pool address is 0x%x nb_mbuf/100 is %d\n",lcore_id,pktmbuf_alloc_pool[lcore_id],nb_alloc_num);
        }
#if 0
        pktmbuf_clone_pool[lcore_id] = rte_pktmbuf_pool_create_by_ops(clone_name,
                 nb_mbuf, 32, 0,
                 RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id(),"ring_sp_sc");
        if (pktmbuf_clone_pool[lcore_id] == NULL){
            rte_exit(EXIT_FAILURE, "Cannot init clone mbuf pool and lcore_id is %d, errno: %d\n",lcore_id,
                 rte_errno);
        }
        else
        {
                TRACE_MEMORY("alloc clone_pktmbuf_pool[%d] success and pool address is 0x%x \n",lcore_id,pktmbuf_clone_pool[lcore_id]);
        }
#else

        pktmbuf_clone_pool[lcore_id] = NULL;//rte_pktmbuf_pool_create_by_ops(clone_name,
#endif
#if 0        
        pktmbuf_loopback_pool[lcore_id] = rte_pktmbuf_pool_create(clone_name,
                 10000, 32, 0,
                 RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
        if (pktmbuf_loopback_pool[lcore_id] == NULL){
            rte_exit(EXIT_FAILURE, "Cannot init loopback mbuf pool and lcore_id is %d, errno: %d\n",lcore_id,
                 rte_errno);
        }
        else
        {
                TRACE_INFO("alloc loopback_pktmbuf_pool[%d] success and pool address is 0x%x \n",lcore_id,pktmbuf_loopback_pool[lcore_id]);
        }
#endif
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
        ret = rte_eth_dev_configure(portid, CONFIG.num_stacks, CONFIG.num_stacks, &port_conf_default);
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
    //    rxconf = &dev_info.default_rxconf;
    //    txconf = &dev_info.default_txconf;
        TRACE_PROC("=============tx offload mask is 0x%x \n============\n",dev_info.tx_offload_capa);
        TRACE_PROC("DEV_TX_OFFLOAD_VLAN_INSERT is 0x%x \n",DEV_TX_OFFLOAD_VLAN_INSERT);
        TRACE_PROC("DEV_TX_OFFLOAD_TCP_CKSUM is 0x%x \n",DEV_TX_OFFLOAD_TCP_CKSUM);
        TRACE_PROC("DEV_TX_OFFLOAD_IPV4_CKSUM ix 0x%x \n",DEV_TX_OFFLOAD_IPV4_CKSUM);
        int rxqueue_id = 0;
        int txqueue_id = 0;
        int port = 0;
        for (rxqueue_id = 0; rxqueue_id < CONFIG.num_stacks; rxqueue_id++) 
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

        for (txqueue_id = 0; txqueue_id < CONFIG.num_stacks; txqueue_id++) {

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
        //
       
        fflush(stdout);
        ret = rte_eth_dev_start(portid);
        if (ret < 0)
        { 

            TRACE_INFO("port %d err and not done: \n",portid);
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
            ret, (unsigned) portid);
        }

       // port_rss_reta_info(portid);
        //init fdir rule
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

	rss_filter_init(CONFIG.num_stacks, 0);
}
/*----------------------------------------------------------------------------*/
static inline int 
dpdk_soft_filter(struct rte_mbuf * pkt_mbuf)
{
#if DRIVER_PRIORITY
	int ret = driver_pri_filter(pkt_mbuf);
	pkt_mbuf->priority = ret;
	return ret;
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

    int num;
    if(level == 0)
    {
         //low private queue        
            num = cirq_count(dpc->tl_mbufs);
    }
    else
    {
        //high private queue
            num = cirq_count(dpc->th_mbufs);
    }
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

    //dpdk_get_start_ave_time(dpc);
    num = dpc->tx_num;
    while(!cirq_empty(dpc->th_mbufs))
    {
        dpc->pkts_txburst[num] = cirq_get(dpc->th_mbufs);//[portid],&dpc->pkts_txburst[num]);
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
#if 0
    uint64_t new_start =  0;
    dpc->check_time++;
    if(dpc->check_last_time == 0)
    { 
        dpc->check_last_time = rte_rdtsc_precise();
    }
    else
    {
        new_start = rte_rdtsc_precise();
        dpc->check_total_time = dpc->check_total_time +  new_start - dpc->check_last_time;
        dpc->check_last_time = new_start;
    }
#endif
#if 0
    dpc->check_time++;
    if(dpc->check_time == 1)
    {
        dpc->check_time = 0;
        do
        {
            ptr  =  n21q_dequeue(dpc->rxfree_queue);
            if(ptr != NULL)
            {
                flag = 1;
                rte_pktmbuf_free(ptr);
            }
            else
            {
                flag = 0;
            }
        }while(flag != 0);
    }
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
//		if(ptr->stream_id != 0) {
//        	TRACE_EXIT("a new test here and ptr->stream_id is %d \n",ptr->stream_id);
//		}
#endif
        if(level == 0)
        {
            if(cirq_add(dpc->rl_mbufs,ptr) != SUCCESS)
            {    
                rte_pktmbuf_free(ptr);
                TRACE_EXIT("dpc->rl_mbufs full and rl count is %d \n",cirq_count(dpc->rl_mbufs));
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
    tag = cirq_get(rx_mbufs);
    if (tag != NULL) {
       	tag->score = dpc->cpu;
		return tag;
    } else {
		TRACE_EXIT("failed to recieve packet from rx_pool\n");
	}
}
#if 0
struct rte_mbuf*  
dpdk_rx_receive_one(struct qstack_context *ctxt, int level, int port)
{

    struct rte_mbuf *tag = NULL;
    struct dpdk_private_context *dpc = dpc_list[ctxt->stack_id];
    
    int portid = 0;

    if(level == 1) {
            tag = cirq_get(dpc->rh_mbufs);
            if(tag == NULL)
            {
                TRACE_INFO("dequeue NULL in rh_mbufs \n");
            }
            else
            {
                tag->score = dpc->cpu;
//                mbuf_t pre = cirq_prefetch(dpc->rh_mbufs);
//                mbuf_prefetch(pre);
            }
            return tag;
    }
    if(level == 0) {
            tag = cirq_get(dpc->rl_mbufs);
            if(tag == NULL)
            {
                TRACE_INFO("dequeue NULL in rl_mbufs \n");
            }
            else
            {
                tag->score = dpc->cpu;
//                q_prefetch0(mbuf_get_buff_ptr(tag));
//                mbuf_t pre = cirq_prefetch(dpc->rl_mbufs);
//                ctxt->mbuf_prefetch = pre;
//                mbuf_prefetch(pre);
            }
            return tag;
    }
    return NULL;
}
#endif

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
#if 0
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(0, &dev_info);
    if ((dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) == 0)
    {
        TRACE_EXIT("IPV4 CKSUM open err \n");
        return -1;
    }
    if ((dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM) == 0)
    {

        TRACE_EXIT("TCP CKSUM open err \n");
        return -2;
    }
#endif
	struct ethhdr *ethh = (struct ethhdr *)rte_pktmbuf_mtod_offset(m, 
			struct ether_hdr *, 0);
	if (unlikely(ntohs(ethh->h_proto)!=ETH_P_IP)) {
		return 1;
	}
    iph = (struct iphdr *)((uint8_t *)ethh + sizeof(struct ether_hdr));
	if (unlikely(iph->protocol != IPPROTO_TCP)) {
		return 1;
	}
    tcph = (struct tcphdr *)((uint8_t *)iph + (iph->ihl<<2));
//    m->l2_len = sizeof(struct ether_hdr);
//    m->l3_len = (iph->ihl<<2);
//    m->l4_len = (tcph->doff<<2);

    m->ol_flags = 0;
    m->ol_flags |= PKT_TX_TCP_CKSUM | PKT_TX_IP_CKSUM | PKT_TX_IPV4 ;
    tcph->check = rte_ipv4_phdr_cksum((struct ipv4_hdr *)iph, m->ol_flags);
    return 1;
}

int   
dpdk_tx_send(struct qstack_context *ctxt, int level, int port, struct rte_mbuf **ptr, int len)
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
    for (i = 0; i< len; i++) {

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

#if 0            
            mbuf = rte_pktmbuf_clone(ptr[i], dpc_src->pktmbuf_clone_pool);
            if (unlikely(mbuf == NULL)) {
                TRACE_EXIT("uwmbuf clone test fail and clone is NULL\n");
                rte_exit(EXIT_FAILURE, "Cannot get clone buf\n");
            }
//            q_prefetch0(mbuf);
        //    mbuf = clone;
            mbuf->tcp_seq = ptr[i]->tcp_seq;
            mbuf->priority = ptr[i]->priority;
            mbuf->payload_len = ptr[i]->payload_len;

            /*do not free ptr[i] after send this packet*/
            /*clone a new mbuf*/
#endif
//            rs_ts_pass(ptr[i], mbuf);
        }
        cirq_add(tx_mbufs,mbuf);
        ret = ret + 1;
        rs_ts_add(mbuf->q_ts, REQ_ST_RSPSENT);
     }
    return ret;
}
#if 0
#endif 

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
            TRACE_EXIT("this is for driver and stack fail and dpc is %x dpc->tx_pktmbuf_pool is %x\n",dpc,dpc->tx_pktmbuf_pool);
            return NULL;
        }

        ptr->score = dpc->cpu;
		ptr->payload_len = 0;
		ptr->tcp_seq = 0;
        return ptr;
    } 
    else 
    { 
        //TRACE_EXIT("this is for app and level is %d\n",level);
        //ptr = n21q_dequeue(dpc->nofree_queue);
        ptr = NULL;
		time = 0;
        while(ptr == NULL)
        {
			time++;
            ptr = n21q_dequeue(dpc->nofree_queue);
            if(ptr == NULL)
            {
               ptr = rte_pktmbuf_alloc(dpc->pktmbuf_alloc_pool);
			   if(ptr != NULL && ptr->udata64 == 2)
			   {
					TRACE_EXIT("================A BIG ERR HERE!!!! FROM MBUFPOOL============\n");
			   }
            } else {

				if(ptr->udata64 == 2)
				{
				TRACE_EXIT("================A BIG ERR HERE FROM N21QUEUE!!!!============\n");
				ptr = NULL;
				} 
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
