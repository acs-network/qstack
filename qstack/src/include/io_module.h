/**
 * @file io_module.h
 * @brief io functions from user-level drivers
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.1
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
 *   9. io_get_rx_err(): packet droped by NIC because of full queue\n
 *	 10. io_get_rx_info_from_nic(): packets isuccessfully received by NIC\n
 *	 11.io_get_rx_backlog(): packet waiting to be processed in the rx_qeuue
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.13
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __IO_MODULE_H_
#define __IO_MODULE_H_
/******************************************************************************/
#include "universal.h"
#include "qstack.h"
/******************************************************************************/
/* function declarations */
/**
 * init io module
 *
 * @return
 *		return SUCCESS if success; otherwise return ERROR
 */
uint8_t 
io_init();

/**
 * recveive packets from NIC's receive queue to receive pool
 *
 * @param qstack 	stack process context
 * @param ifidx 	NIC port
 * @param cur_ts_us	current system time in us
 *
 * @retrun
 *		return the total num of packets queueing in the receive pool
 */
uint32_t
io_recv_check(qstack_t qstack, int ifidx, systs_t cur_ts_us);

/**
 * get one mbuf from receive queue
 *
 * @param qstack 	stack process context
 * @param ifidx 	NIC port
 * @param[out] 		len length of packet
 * 
 * @return
 *		return one mbuf if success; otherwise return NULL
 */
mbuf_t
io_recv_mbuf(qstack_t qstack, int ifidx, uint16_t *len);

/**
 * send packets from send pool to NIC's receive queue
 *
 * @param qstack 	stack process context
 * @param ifidx 	NIC port
 *
 * @return
 * 	the num of packets sent to NIC
 */
uint32_t
io_send_check(qstack_t qstack, int ifidx);

/**
 * sendout a mbuf to driver's TX queue
 *
 * @param qstack 	stack process context
 * @param ifidx 	NIC port
 * @param mbuf 		target mbuf
 *
 * @return
 *	return SUCCESS if success; otherwise return NULL
 */
uint8_t
io_send_mbuf(qstack_t qstack, int ifidx, mbuf_t mbuf, int len);

/**
 * get a free mbuf for stack to send out a packet
 *
 * @param core_id 	stack process context core id
 * 					in 20181205 new  version is the target core id
 * @param ifidx 	NIC port
 *
 * @return
 *	return empty mbuf if success; otherwise return NULL
 *
 * @note remember to write pkt_len and data_len before send out mbuf!
 */
mbuf_t
io_get_swmbuf(int core_id, int ifidx);

/**
 * get a free mbuf for user to send out a packet
 *
 * @param app 	    application context 
 * 					in 20181205 new  version is the target core id
 * @param ifidx 	NIC port
 *
 * @return
 *	return empty mbuf if success; otherwise return NULL
 *
 * @note remember to write pkt_len and data_len before send out mbuf!
 */ 
mbuf_t
io_get_uwmbuf(qapp_t app, int ifidx);

/**
 * get a free mbuf to be writen and sent out
 *
 * @param app 			application context
 * @param[out] buff 	return the pointer pointing to position to write tcp 
 * 	payload in the returned mbuf
 * @param[out] 			max_len max length of payload to be writen in
 *
 * @return
 * 	return a writable mbuf if success; otherwise return NULL
 */
static inline mbuf_t
io_get_wmbuf(qapp_t app, uint8_t **buff, int *max_len, int hold)
{
	mbuf_t mbuf;
	int core_id;
    core_id = app->core_id;
	if (hold) {
		mbuf = io_get_uwmbuf(app, 0);
	} else {
		mbuf = io_get_swmbuf(core_id, 0);
	}
	if (mbuf) {
		if (mbuf->mbuf_state != MBUF_STATE_FREE) {
			    TRACE_EXCP("mbuf state = %d\n", mbuf->mbuf_state);
			    TRACE_EXCP("max len = %d, core_id = %d\n", *max_len, core_id);
			    TRACE_ERROR("get wrong uwmbuf!\n");
		}
		mbuf->mbuf_state = MBUF_STATE_TALLOC;
		TRACE_MBUFPOOL("alloc mbuf %p @ Core %d for user\n ", 
				mbuf, core_id);
		*buff = (uint8_t *)mbuf_get_tcp_ptr(mbuf) + 20;
		*max_len = 1460;
	} else {
		TRACE_EXCP("failed to alloc wmbuf at Core %d! type:%d\n", 
				core_id, hold);
	}
	return mbuf;
}

/**
 * free a mbuf into its mempool
 *
 * @param core_id 	stack process context core id
 * @param mbuf 		target mbuf
 *
 * @return null
 */
void 
io_free_mbuf(int core_id, mbuf_t mbuf);

/**
 * get two times check last time
 *
 * @param cpu  == 0 
 *
 * @return num
  */
int 
io_get_rx_last_time(int cpu);
/** 
 * get rx err packet num
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return num
  */
int 
io_get_rx_err(int port);

/**
 * get rx err packet num if no buf space to rx ring
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return num
  */
int 
io_get_rx_nobuf_err(int port);

 /**
 * get rx success packet num
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return num
 */
int
io_get_rx_info_from_nic(int port);

/**
 * get the number of packets waiting to be processed in the rx_queue
 *
 * @param qstack 	stack thread context
 *
 * @return
 *	the number of packets remain in rx_queue
 */
int
io_get_rx_backlog(qstack_t qstack);

/**
 * get the number of packets waiting to be sent in the tx_queue
 *
 * @param qstack	stack thread context
 *
 * @return
 * 	the number of packet remain in rx_queue
 */
int 
io_get_tx_backlog(qstack_t qstack);
/******************************************************************************/
/**
 * only for test 
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return 1 if success
 */
int 
io_get_rxtx_ten(qstack_t qstack);



 /**
 * get tx success packet num
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return num
 */
int
io_get_tx_out(int port);


 /**
 * get tx err packet num
 *
 * @param port 	eth port num,assert port == 0 
 *
 * @return num
 */
int
io_get_tx_err(int port);

int io_get_tx_alloc_pool(qstack_t qstack);


 /**
 * update fdir function
 * @param port_id: target nic
 * @param ruleID: change ruleID,if targe rule is NULL,make a new rule
 * @param core_id: target core ID
 *
 * @return num
 */
int 
io_update_fdir_function(int port_id,int ruleID,int core_id);

#endif //#ifndef __IO_MODULE_H_
