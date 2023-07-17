 /**
 * @file io_module.c
 * @brief io functions from user-level drivers
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.2
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.19
 *       Author: Shen Yifan
 *       Modification: create
 *   2. Date:
 *       Author:
 *       Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//    #define TRACE_LEVEL    TRACELV_DETAIL
#endif
//#define FULL_CIRQ_ISOK
/******************************************************************************/
#include "io_module.h"
#include "dpdk_module.h"
#include "timestamp.h"
/******************************************************************************/
/* local macros */
#if LOOP_BACK_TEST_MODE
	#define VIRTUAL_FLOW_NUM		8000
	#define VIRTUAL_RX_QUEUE_LEN	100000
	#define VIRTUAL_TX_QUEUE_LEN	100000
	#define VIRTUAL_REQ_LEN			20
#endif
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
#if LOOP_BACK_TEST_MODE
struct circular_queue rx_queue;
struct circular_queue tx_queue;
struct rte_mempool * free_mbuf_pool; // used for stack input
struct rte_mempool * swmbuf_pool; // used for stack output
struct rte_mempool * uwmbuf_pool; // used for app output

struct stream_state
{
	uint32_t stream_id;
	uint32_t state;
	mbuf_t syn;
	mbuf_t ack;
	mbuf_t data;
	uint32_t seq_num;
	uint32_t ack_seq;
};
struct stream_state vstreams[VIRTUAL_FLOW_NUM];
#endif
/******************************************************************************/
/* local static functions */
#if LOOP_BACK_TEST_MODE
static inline void
print_vstream_info(struct stream_state *vstream)
{
	TRACE_STREAM("stream state: %d, seq_num: %u, ack_seq: %u\n", 
			vstream->state, vstream->seq_num, vstream->ack_seq);
}

static inline void
init_virtual_syn(struct stream_state *vstream)
{
	mbuf_t mbuf = vstream->syn;
	
	// set ethernet header
	struct ethhdr *ethh = (struct ethhdr *)mbuf_get_buff_ptr(mbuf);
	ethh->h_proto = htons(ETH_P_IP);
	mbuf->l2_len = sizeof(struct ethhdr);

	// set ip header
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	iph->ihl = IP_HEADER_LEN >> 2;
	mbuf->l3_len = IP_HEADER_LEN;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + TCP_HEADER_LEN + 8);
	iph->id = 0; // not checked
	iph->frag_off = htons(IP_DF);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = vstream->stream_id >> 16;
	iph->daddr = 1234; // not checked
	iph->check = 0;
//	iph->check = ip_fast_csum(iph, iph->ihl); 
	
	// set tcp header
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	int optlen = 8;
	mbuf->l4_len = (TCP_HEADER_LEN + optlen);
	mbuf->payload_len = 0;
	memset(tcph, 0, TCP_HEADER_LEN + optlen);
	tcph->source = htons(vstream->stream_id & 0xffff);
	tcph->dest = htons(80); // not checked
	tcph->syn = TRUE;
	tcph->seq = htonl(vstream->seq_num);
	tcph->ack_seq = htonl(vstream->ack_seq);
	tcph->window = htons((uint16_t)1024);
	tcph->doff = (TCP_HEADER_LEN + optlen) >> 2;
	uint8_t *tcpopt = (uint8_t *)tcph + TCP_HEADER_LEN;
	tcpopt[0] = TCP_OPT_MSS;
	tcpopt[1] = TCP_OPT_MSS_LEN;
	tcpopt[2] = 1500 >> 8;
	tcpopt[3] = 1500 % 256;
	tcpopt[4] = TCP_OPT_NOP;
	tcpopt[5] = TCP_OPT_WSCALE;
	tcpopt[6] = TCP_OPT_WSCALE_LEN;
	tcpopt[7] = 0;
//	tcph->check = tcp_csum((uint16_t *)tcph, 
//			TCP_HEADER_LEN + optlen + payloadlen, 
//		    cur_stream->saddr, cur_stream->daddr);
	mbuf->pkt_len = mbuf->data_len = mbuf->pkt_len = mbuf->l2_len + 
			mbuf->l3_len + mbuf->l4_len + mbuf->payload_len;
}

static inline void
init_virtual_ack(struct stream_state *vstream)
{
	mbuf_t mbuf = vstream->ack;
	
	// set ethernet header
	struct ethhdr *ethh = (struct ethhdr *)mbuf_get_buff_ptr(mbuf);
	ethh->h_proto = htons(ETH_P_IP);
	mbuf->l2_len = sizeof(struct ethhdr);

	// set ip header
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	iph->ihl = IP_HEADER_LEN >> 2;
	mbuf->l3_len = IP_HEADER_LEN;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + TCP_HEADER_LEN);
	iph->id = 0; // not checked
	iph->frag_off = htons(IP_DF);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = vstream->stream_id >> 16;
	iph->daddr = 1234; // not checked
	iph->check = 0;
//	iph->check = ip_fast_csum(iph, iph->ihl);
	
	// set tcp header
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	int optlen = 0;
	mbuf->l4_len = (TCP_HEADER_LEN + optlen);
	mbuf->payload_len = 0;
	memset(tcph, 0, TCP_HEADER_LEN + optlen);
	tcph->source = htons(vstream->stream_id & 0xffff);
	tcph->dest = htons(80); // not checked
	tcph->ack = TRUE;
	tcph->seq = htonl(vstream->seq_num);
	tcph->ack_seq = htonl(vstream->ack_seq);
	tcph->window = htons((uint16_t)1024);
	tcph->doff = (TCP_HEADER_LEN + optlen) >> 2;
//	tcph->check = tcp_csum((uint16_t *)tcph, 
//			TCP_HEADER_LEN + optlen + payloadlen, 
//		    cur_stream->saddr, cur_stream->daddr);
	mbuf->pkt_len = mbuf->data_len = mbuf->pkt_len = mbuf->l2_len + 
			mbuf->l3_len + mbuf->l4_len + mbuf->payload_len;
}

static inline void
init_virtual_data(struct stream_state *vstream)
{
	mbuf_t mbuf = vstream->data;
	
	// set ethernet header
	struct ethhdr *ethh = (struct ethhdr *)mbuf_get_buff_ptr(mbuf);
	ethh->h_proto = htons(ETH_P_IP);
	mbuf->l2_len = sizeof(struct ethhdr);
	static count = 0;

	// set ip header
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	iph->ihl = IP_HEADER_LEN >> 2;
	mbuf->l3_len = IP_HEADER_LEN;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + TCP_HEADER_LEN + VIRTUAL_REQ_LEN);
	iph->id = 0; // not checked
	iph->frag_off = htons(IP_DF);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = vstream->stream_id >> 16;
	iph->daddr = 1234; // not checked
	iph->check = 0;
//	iph->check = ip_fast_csum(iph, iph->ihl);
	
	// set tcp header
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	int optlen = 0;
	mbuf->l4_len = (TCP_HEADER_LEN + optlen);
	mbuf->payload_len = VIRTUAL_REQ_LEN;
	memset(tcph, 0, TCP_HEADER_LEN + optlen);
	tcph->source = htons(vstream->stream_id & 0xffff);
	tcph->dest = htons(80); // not checked
	tcph->ack = TRUE;
	tcph->seq = htonl(vstream->seq_num);
	tcph->ack_seq = htonl(vstream->ack_seq);
	tcph->window = htons((uint16_t)1024);
	tcph->doff = (TCP_HEADER_LEN + optlen) >> 2;
	uint8_t *payload = (uint8_t *)tcph + mbuf->l4_len;
	if (count++ == 20) {
		payload[5] = 0x01;
		count = 0;
	} else {
		payload[5] = 0;
	}
//	tcph->check = tcp_csum((uint16_t *)tcph, 
//			TCP_HEADER_LEN + optlen + payloadlen, 
//		    cur_stream->saddr, cur_stream->daddr);
	mbuf->pkt_len = mbuf->data_len = mbuf->pkt_len = mbuf->l2_len + 
			mbuf->l3_len + mbuf->l4_len + mbuf->payload_len;
}

static inline void
update_virtual_mbuf(struct stream_state *vstream, mbuf_t mbuf)
{
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	tcph->seq = htonl(vstream->seq_num);
	tcph->ack_seq = htonl(vstream->ack_seq);
}

static inline void
send_virtual_syn(struct stream_state *vstream)
{
	while (cirq_add(&rx_queue, vstream->syn) != SUCCESS);
	TRACE_CNCT("syn packet with seq: %u\tsent @ VStream %d\n", 
			vstream->seq_num, vstream->stream_id);
	vstream->state = TCP_ST_SYN_SENT;
	vstream->seq_num++;
}

static inline void
send_virtual_ack(struct stream_state *vstream)
{
	TRACE_MBUF("try to send ack @ VStream %d\n seq_num: %u, ack_seq: %u\n", 
			vstream->stream_id, vstream->seq_num, vstream->ack_seq);
	update_virtual_mbuf(vstream, vstream->ack);
	while (cirq_add(&rx_queue, vstream->ack) != SUCCESS);
}

static inline void
send_virtual_data(struct stream_state *vstream)
{
	TRACE_MBUF("try to send request @ VStream %d\n"
			" seq_num: %u, ack_seq: %u, payload_len: %d\n", 
			vstream->stream_id, vstream->seq_num, vstream->ack_seq, 
			vstream->data->payload_len);
	update_virtual_mbuf(vstream, vstream->data);
	while (cirq_add(&rx_queue, vstream->data) != SUCCESS);
	vstream->seq_num += VIRTUAL_REQ_LEN;
	rs_ts_start(&get_global_ctx()->stack_contexts[0]->req_stage_ts, 
			vstream->data); 
}
/******************************************************************************/
static inline void
process_loopback_packet(mbuf_t mbuf)
{
	struct iphdr *iph = (struct iphdr *)mbuf_get_ip_ptr(mbuf);
	struct tcphdr *tcph = (struct tcphdr *)mbuf_get_tcp_ptr(mbuf);
	uint32_t stream_id = (iph->daddr << 16) + ntohs(tcph->dest);
	struct stream_state *vstream = &vstreams[stream_id];
	
	rs_ts_check(mbuf);
	TRACE_CHECKP("process loopback packet @ VStream %d!\n", stream_id);
	print_vstream_info(vstream);
	if (vstream->state == TCP_ST_SYN_SENT) { // it's a syn ack
		if (tcph->syn && tcph->ack) {
			TRACE_CHECKP("SYNACK received @ VStream %d!\n", stream_id);
			vstream->state = TCP_ST_ESTABLISHED;
			vstream->ack_seq = ntohl(tcph->seq) + 1;
			send_virtual_ack(vstream);
			send_virtual_data(vstream);
			rte_pktmbuf_free(mbuf);
		} else {
			TRACE_EXIT("unexcepted mbuf at TCP_ST_SYN_SENT!\n");
		}
	} else if (vstream->state == TCP_ST_ESTABLISHED) {
		if (mbuf->payload_len) { // it's a response packet
			TRACE_CHECKP("response received @ VStream %d!\n"
					"mbuf len: %d, seq_num: %u, ack_seq: %u\n", 
					stream_id, mbuf->payload_len, ntohl(tcph->seq), 
					ntohl(tcph->ack_seq));
			vstream->ack_seq = ntohl(tcph->seq) + mbuf->payload_len;
			send_virtual_ack(vstream);
			send_virtual_data(vstream);
		} else { // it's an ack packet
			vstream->ack_seq = ntohl(tcph->seq);
#ifdef DISABLE_SERVER
	#ifndef VIRTUAL_SERVER
			// if the server is disabled and the virtual server is not 
			// available, there would be no responses sent back
			send_virtual_data(vstream);
	#endif
#endif
			rte_pktmbuf_free(mbuf);
		}
	}
}

static void
unit_test_loopback_mainloop()
{
	uint32_t connect_num = 0;
	mbuf_t mbuf = NULL;
	sleep(10);
	while (1) {
		if (mbuf = cirq_get(&tx_queue)) {
			process_loopback_packet(mbuf);
		}
		if (connect_num < VIRTUAL_FLOW_NUM) {
			send_virtual_syn(&vstreams[connect_num]);
			connect_num++;
		}
	}
}
#endif
/******************************************************************************/
/* functions */
/******************************************************************************/

	
	
uint8_t
io_init()
{
    int ret = 0;
    int cpu = 1;
    int num_mem_ch = 4;
    uint32_t cpumask = 0;
    char cpumaskbuf[10];
    char mem_channels[5];
    /* get the cpu mask */
    for (ret = 0; ret < cpu; ret++){
        cpumask = (cpumask | (1 << ret));
    }

    sprintf(cpumaskbuf, "%X", cpumask);
    sprintf(mem_channels, "%d", num_mem_ch);


    char *argv[] = {"",
            "-c",
            cpumaskbuf,
            "-n",
            mem_channels,
            "--proc-type=auto",
            ""
    };


    const int argc = 6;
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        TRACE_ERR("rte_eal_init() failed!\n");
        exit(0);
    }


#if !LOOP_BACK_TEST_MODE
    dpdk_load_module();
    int core_id = 0;
    for(core_id = 0; core_id < MAX_DPC_THREAD;core_id++){
        dpdk_init_handle(core_id);
     }
    return SUCCESS;
/*init io module*/
#else
	int i;
    char pool_name[RTE_MEMPOOL_NAMESIZE];
	cirq_init(&rx_queue, VIRTUAL_RX_QUEUE_LEN);
	cirq_init(&tx_queue, VIRTUAL_TX_QUEUE_LEN);
    sprintf(pool_name, "free_mbuf_pool");
	free_mbuf_pool = rte_pktmbuf_pool_create(pool_name, 
			MAX(VIRTUAL_FLOW_NUM*3, 1024), 32, 
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    sprintf(pool_name, "swmbuf_pool");
	swmbuf_pool = rte_pktmbuf_pool_create(pool_name, 
			MAX(VIRTUAL_FLOW_NUM, 1024), 32, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    sprintf(pool_name, "uwmbuf_pool");
	uwmbuf_pool = rte_pktmbuf_pool_create(pool_name, 
			MAX(VIRTUAL_FLOW_NUM, 1024), 32, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (!(free_mbuf_pool && swmbuf_pool && uwmbuf_pool)) {
		TRACE_EXIT("mbuf pool init failed!\n");
	}
	for (i=0; i<VIRTUAL_FLOW_NUM; i++) {
		vstreams[i].stream_id = i;
		vstreams[i].state = TCP_ST_CLOSED;
		vstreams[i].syn = rte_pktmbuf_alloc(free_mbuf_pool);
		vstreams[i].ack = rte_pktmbuf_alloc(free_mbuf_pool);
		vstreams[i].data = rte_pktmbuf_alloc(free_mbuf_pool);
		vstreams[i].seq_num = 0;
		vstreams[i].ack_seq = 0;

		init_virtual_syn(&vstreams[i]);
		init_virtual_ack(&vstreams[i]);
		init_virtual_data(&vstreams[i]);
	}
    
    pthread_t loop_thread;
	pthread_attr_t attr;
    cpu_set_t cpus;
    pthread_attr_init(&attr);
	CPU_ZERO(&cpus);
	CPU_SET(3 , &cpus);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
	pthread_create(&loop_thread, &attr, unit_test_loopback_mainloop, NULL);
#endif;
}

//#define IO_RECV_CHECK_TIMEOUT_TEST
#ifdef IO_RECV_CHECK_TIMEOUT_TEST
    #define IO_RECV_CHECK_TIMEOUT_THRESH    1000
#endif
uint32_t
io_recv_check(qstack_t qstack, int ifidx, systs_t cur_ts_us)
{
#if !LOOP_BACK_TEST_MODE
	#ifdef IO_RECV_CHECK_TIMEOUT_TEST
    uint64_t start_ts = get_time_us();
	#endif
	DSTAT_CHECK_ADD(qstack->req_add_num, qstack->recv_check_call, 1);
    assert(ifidx == 0); // ifidx is 0 for debug
     /*: get mbufs from NIC's receive queue to recieve pool\n*/
    int ret = 0;
    
	#ifdef CHECK_INSERT
	if (cur_ts_us == FETCH_NEW_TS) {
		cur_ts_us = get_time_us();
	}
	qstack->rt_ctx->last_check_ts = cur_ts_us;
	#endif
	ret = dpdk_check_rx_ring(qstack,qstack->stack_id);
	if(ret == 0)
	{
		DSTAT_CHECK_ADD(qstack->req_add_num, qstack->recv_check_call_zero, 1);
	}
	#ifdef IO_RECV_CHECK_TIMEOUT_TEST
    uint64_t interval = get_time_us() - start_ts;
    if (interval> IO_RECV_CHECK_TIMEOUT_THRESH) {
        TRACE_EXCP("io_recv_check() timeout: %llu, rcved: %d, NIC drop:%u\n", 
                interval, ret, io_get_rx_err(0));
    }
	#endif

	return ret;
#else
	return cirq_count(&rx_queue);
#endif
}

mbuf_t
io_recv_mbuf(qstack_t qstack, int ifidx, uint16_t *len)
{
    struct rte_mbuf *point;
	DSTAT_ADD(qstack->recv_mbuf_call, 1);
#if !LOOP_BACK_TEST_MODE
	/* try to recieve from high-pri queue first */
    point =  dpdk_rx_receive_one(qstack,1,0);
    if (point == NULL) {
        point =  dpdk_rx_receive_one(qstack,0,0);
    }
#else
	point = cirq_get(&rx_queue);
#endif
	if (unlikely(!point)) {
		TRACE_ERROR("failed to get mbuf from io_recv_mbuf!\n");
	}
    if (len) {
		*len = point->pkt_len;
    }
    BSTAT_ADD(qstack->pkt_in, 1);
    BSTAT_ADD(qstack->byte_in, point->data_len);
    return point;
}

uint32_t
io_send_check(qstack_t qstack, int ifidx)
{
#if !LOOP_BACK_TEST_MODE
    assert(ifidx == 0); // ifidx is 0 for debug
     /*: send mbufs from send pool to NIC's send queue\n*/
    int tx_num = 0;
    tx_num = dpdk_check_tx_ring(qstack,qstack->stack_id);
    return tx_num;
#else
	return 0;
#endif
}

// *   5. io_send_mbuf: send mbufs from user to send pool\n
uint8_t
io_send_mbuf(qstack_t qstack, int pri, mbuf_t mbuf, int len)
{
	if (unlikely(!mbuf)) {
		TRACE_ERROR("try to send an empty mbuf!\n");
	}
	// TODO: trate all packet out as high-pri for test 
	pri = 1;	
	BSTAT_ADD(qstack->pkt_out, 1);
	BSTAT_ADD(qstack->byte_out, mbuf->data_len);
	if (mbuf->data_len == 0) {
		mbuf->data_len = len;
	}
	if (mbuf->pkt_len == 0) {
		mbuf->pkt_len = len;
	}
#if !LOOP_BACK_TEST_MODE
	return dpdk_tx_send(qstack, pri, 0, &mbuf, 1);
#else 
	return cirq_add(&tx_queue, mbuf);
#endif
}

// *   6. io_get_uwmbuf(): get mbuf to be writen by user\n
mbuf_t
io_get_uwmbuf(qapp_t app, int ifidx)
{
	int core_id = app->core_id;
	DSTAT_ADD(get_global_ctx()->uwmbuf_alloced[app->app_id], 1);
#if !LOOP_BACK_TEST_MODE
//  can not free
//    qstack_t qstack = NULL;
//    qstack = get_stack_context(core_id);
    struct rte_mbuf *ret = NULL;
    ret = dpdk_alloc_mbuf(core_id,1);
    if(ret == NULL){
        TRACE_EXCP("get uwmbuf fail\n");
        return NULL;    
    }
	mbuf_set_op(ret, MBUF_OP_SND_UALLOC, core_id);
    return ret;
#else
	return rte_pktmbuf_alloc(uwmbuf_pool);
#endif
}

mbuf_t
io_get_swmbuf(int core_id, int ifidx)
{
#if !LOOP_BACK_TEST_MODE
//can free
    struct rte_mbuf *ret = NULL;
    ret = dpdk_alloc_mbuf(core_id,0);
    if(ret == NULL){
        TRACE_EXCP("get swmbuf fail\n");
        return NULL;
    }
	mbuf_set_op(ret, MBUF_OP_SND_SALLOC, core_id);
    return ret;
//    return dpdk_alloc_mbuf(core_id,0);
#else
	return rte_pktmbuf_alloc(swmbuf_pool);
#endif
}

void 
io_free_mbuf(int core_id, mbuf_t mbuf)
{
//    qstack_t qstack = NULL;
//    qstack = get_stack_context(core_id);
	rs_ts_clear(mbuf);
#if MBUF_STREAMID
	mbuf->stream_id = 0;
#endif
#if !LOOP_BACK_TEST_MODE
    dpdk_release_pkt(core_id,0,mbuf);
#else
	if (mbuf->pool != free_mbuf_pool) {
		rte_pktmbuf_free(mbuf);
	}
#endif
}

void *
io_checkloops(void *args)
{
    int max_stacks = CONFIG.stack_thread;
    //assert(max_stack == 2);
    int i = 0;
    while(1){
        for(i= 0;i<max_stacks;i++){
            dpdk_check_rx_ring(NULL,i);
            dpdk_check_tx_ring(NULL,i);
        }
    }
}

int 
io_get_rxtx_ten(qstack_t qstack)
{
	/*
    pthread_t *fd;
    pthread_attr_t attr;
    cpu_set_t cpus;
    pthread_attr_init(&attr);
    CPU_ZERO(&cpus);
    CPU_SET(9, &cpus);
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);
    pthread_create(&fd, &attr, io_checkloops, NULL);
    return 1;
	*/
}

int 
io_get_rx_info_from_nic(int port)
{
    assert(port == 0);

    struct rte_eth_stats stats;
    int ret = -1;
    ret = rte_eth_stats_get(port,&stats);
    assert(ret == 0);
    return stats.ipackets;

}

int
io_get_rx_last_time(int cpu)
{ 
    return dpdk_get_rx_last_time(cpu);
}

//io_get_rx_backlog(qstack_t qstack)
int io_get_tx_alloc_pool(qstack_t qstack) 
{
	return dpdk_get_mempool_state(qstack, 1);
}

int 
io_get_rx_err(int port)
{
#if !LOOP_BACK_TEST_MODE
    struct rte_eth_stats stats;
    int ret = -1;
    ret = rte_eth_stats_get(port,&stats);
    assert(ret == 0);
    return stats.imissed;
#else
	return 0;
#endif
}

int 
io_get_rx_nobuf_err(int port)
{
//#if !LOOP_BACK_TEST_MODE
    struct rte_eth_stats stats;
    int ret = -1;
    ret = rte_eth_stats_get(port,&stats);
    assert(ret == 0);
    return stats.rx_nombuf;
//#else
//	return 0;
//#endif
}
int 
io_get_tx_out(int port)
{
#if !LOOP_BACK_TEST_MODE
    struct rte_eth_stats stats;
    int ret = -1;
    ret = rte_eth_stats_get(port,&stats);
    assert(ret == 0);
    return stats.opackets;
#else
	return 0;
#endif
} 

int 
io_get_tx_err(int port)
{
#if !LOOP_BACK_TEST_MODE
    struct rte_eth_stats stats;
    int ret = -1;
    ret = rte_eth_stats_get(port,&stats);
    assert(ret == 0);
    return stats.oerrors;
#else
	return 0;
#endif
}

int 
io_get_rx_backlog(qstack_t qstack)
{
#if !LOOP_BACK_TEST_MODE
	return dpdk_get_rx_state(qstack,0,0) + dpdk_get_rx_state(qstack,1,0);
#else
	return cirq_count(&rx_queue);
#endif
}


int 
io_update_fdir_function(int port_id ,int ruleID,int core_id)
{
#if !LOOP_BACK_TEST_MODE

	return 1;
//	return dpdk_update_fdir_function(port_id,ruleID,core_id);
#else
	return 1;
#endif
}

/******************************************************************************/
