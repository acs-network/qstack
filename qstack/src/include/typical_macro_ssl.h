/**
 * @file global_macro.h
 * @brief global macros for the whole system
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.10.14
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.10.14
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TYPICAL_MACRO_SSL_H_
#define __TYPICAL_MACRO_SSL_H_
/******************************************************************************/
/* global macros */
// these should only be used at array static allocation
#define MAX_CORE_NUM 	1
#define MAX_STACK_NUM 	1
#define MAX_SERVER_NUM 	1
#define MAX_APP_NUM			MAX_CORE_NUM
#define CRYP_THREAD_CORE	MAX_CORE_NUM+0 ///< dedicated core to cryption
#define MONITOR_THREAD_CORE	MAX_CORE_NUM+1	///< dedicated core for monitor
#define MSG_THREAD_CORE		MAX_CORE_NUM+2	///< dedicated core to flush message
#define LOG_THREAD_CORE		MAX_CORE_NUM+3	///< dedicated core to flush log

#define MAX_FLOW_NUM	5000000
#define MEM_SCALE		4		///< use less memory of 2^MEM_SCALE
/*----------------------------------------------------------------------------*/
#define SHARED_NOTHING_MODE			

// compile controllers
#define MAX_RTO_BATCH			10 // proess at most 10 rto every main loop
#define MAX_RECV_BATCH			32
#define MAX_CLOSE_BATCH			10
#define MAX_TX_CHECK_BATCH		10
#define MAX_CTRL_PKT_BATCH		MAX_RECV_BATCH*2
#define MAX_DATA_PKT_BATCH		MAX_RECV_BATCH*2
#define MAX_ACK_PKT_BATCH		MAX_RECV_BATCH*2
#define EXTRA_RECV_CHECK		0	/* insert extra rx_check into main_loop.
									   set 1 after recv processing and app call
									   set 2 after timer and generate send pkt
									*/
/*----------------------------------------------------------------------------*/
#define STATISTIC_STATE_DETAIL			1	///< detail statistic
#define INSTACK_TLS						1	///< enable in-satck ssl support
/******************************************************************************/
#endif //#ifdef __TYPICAL_MACRO_SSL_H_
