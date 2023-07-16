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
 *   2. Date: 2020.5.12
 *   	Author: Shen Yifan
 *   	Modification: using default and typical macros
 *   3. Date: 
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __GLOBAL_MACRO_H_
#define __GLOBAL_MACRO_H_
/******************************************************************************/
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>  
#include <errno.h>
/******************************************************************************/
//#include "typical_macro_echo.h"
#include "typical_macro.h"
/******************************************************************************/
/* default macros */
#ifndef MAX_CORE_NUM
	#define MAX_CORE_NUM 	8
#endif
#ifndef MAX_STACK_NUM
	#define MAX_STACK_NUM 	8
#endif
#ifndef MAX_SERVER_NUM
	#define MAX_SERVER_NUM 	8
#endif
#ifndef MAX_APP_NUM
	#define MAX_APP_NUM			MAX_CORE_NUM
#endif
	
#ifndef MSG_THREAD_CORE
	#define MSG_THREAD_CORE		MAX_CORE_NUM+0	///< dedicated core to flush msg
#endif
#ifndef LOG_THREAD_CORE
	#define LOG_THREAD_CORE		MAX_CORE_NUM+1	///< dedicated core to flush log
#endif
//#ifndef MONITOR_THREAD_CORE
//	#define MONITOR_THREAD_CORE	MAX_CORE_NUM+2	///< dedicated core for monitor
//#endif
	
#ifndef MAX_FLOW_NUM
	#define MAX_FLOW_NUM	5000000
#endif
#ifndef MEM_SCALE
	#define MEM_SCALE		4		///< use less memory of 2^MEM_SCALE
#endif
/******************************************************************************/
/*----------------------------------------------------------------------------*/
// priority control
#ifndef PRIORITY_RECV
	#define PRIORITY_RECV					1	///< enalbe tcp recieve priority
	#if PRIORITY_RECV
		#define PRIORITY_RECV_BUFF				///< use priority receive buffer
	
	//	#define UNIFORM_PRIORITY			1 	// comment to disable it
		#define DRIVER_PRIORITY				1	///< identify priority at driver
		#define NIC_PRIORITY				0	///< identifu priority at NIC
		#define TCP_PRIORITY				0	///< identify priority at trasport
		#define USER_DEFINED_PRIORITY		0	///< identify priority at payload
	#endif
#endif
#ifndef PRIORITY_SEND
	#define PRIORITY_SEND					1	///< enalbe tcp send priority
#endif
	
// threading configurations
#ifndef CONTEXT_SWITCH
	#define CONTEXT_SWITCH					1
	#if CONTEXT_SWITCH
		#define COROUTINE_SCHEDULING
		#ifdef COROUTINE_SCHEDULING
	//		#define PREEMPTIVE_MODE
		#else
			#define	PTHREAD_SCHEDULING
	//		#define STACK_ACTIVE_YIELD				///< stack thread yield if idle 
		#endif
	
		#define CHECK_INSERT
		#ifdef CHECK_INSERT
			#define STACK_PENDING_TIMEOUT			1000	///< us
			#define	STACK_CHECK_TIMEOUT				200		///< us
			#define EVENT_CHECK_TIMEOUT				200		///< us
			#define RT_CHECK_TIMEOUT				10000	///< complex app thread
		#endif 
	#endif
#endif	

// TCP timer control
#ifndef USING_TCP_TIMER
	#define USING_TCP_TIMER					1	///< using tcp timers
	#if USING_TCP_TIMER
		#define TCP_TIMER_TIMEOUT			1
		#define TCP_TIMER_TIMEWAIT			1
		#define	TCP_TIMER_RTO				1	
		#if TCP_TIMER_RTO
			#define	TCP_SIMPLE_RTO
		#endif
	#endif
#endif

#ifndef VIRTUAL_TASK_DELAY_MODE
	#define VIRTUAL_TASK_DELAY_MODE			1
#endif

// loopback test mode without NIC and driver
#ifndef LOOP_BACK_TEST_MODE
	#define LOOP_BACK_TEST_MODE				0
#endif
#ifndef DISABLE_SERVER
	#define DISABLE_SERVER					0
	#ifdef DISABLE_SERVER
		#define VIRTUAL_SERVER			
	#else
	//	#define DONOT_RESPONSE
		#ifndef DONOT_RESPONSE
			#define ALWAYS_RESPONSE
		#endif
	#endif
#endif

// batching limitation for packet drop avoidance
#ifndef MAX_RTO_BATCH	
	#define MAX_RTO_BATCH			5 // proess at most 10 rto every main loop
#endif
#ifndef MAX_RECV_BATCH
	#define MAX_RECV_BATCH			32
#endif
#ifndef MAX_CLOSE_BATCH
	#define MAX_CLOSE_BATCH			5
#endif
#ifndef MAX_CONNECT_BATCH
	#define MAX_CONNECT_BATCH		5
#endif
#ifndef MAX_TX_CHECK_BATCH
	#define MAX_TX_CHECK_BATCH		5
#endif
#ifndef MAX_CTRL_PKT_BATCH
	#define MAX_CTRL_PKT_BATCH		MAX_RECV_BATCH
#endif
#ifndef MAX_DATA_PKT_BATCH
	#define MAX_DATA_PKT_BATCH		MAX_RECV_BATCH
#endif
#ifndef MAX_ACK_PKT_BATCH
	#define MAX_ACK_PKT_BATCH		MAX_RECV_BATCH
#endif
#ifndef EXTRA_RECV_CHECK
	#define EXTRA_RECV_CHECK		0	/* insert extra rx_check into main_loop.
										   set 1 after recv processing and app call
										   set 2 after timer and generate send pkt
										*/
#endif
#ifndef SCAN_INACTIVE_SOCKET
	#define SCAN_INACTIVE_SOCKET			0
#endif
/*----------------------------------------------------------------------------*/
// statistic
#ifndef STATISTIC_STATE_BASIC
	#define STATISTIC_STATE_BASIC			1	///< basic statistic
#endif
#ifndef STATISTIC_STATE_DETAIL
	#define STATISTIC_STATE_DETAIL			0	///< detail statistic
#endif
#ifndef STATISTIC_TEMP
	#define STATISTIC_TEMP					0	///< temp statistic
	#if STATISTIC_TEMP
		#define STAT_TEMP_NUM				2
	#endif
#endif
#ifndef STATISTIC_STATE
	#define STATISTIC_STATE					1	///< statistic network stack state
	#if STATISTIC_STATE
		#define STATE_PRINT_INTERVAL			1	///< state print interval (s)
	#endif
	#if SCAN_INACTIVE_SOCKET
		#define INACTIVE_THRESH					300000 // inactive timeout thresh (ms)
		#define INACTIVE_EXIT_THRESH			10000 // exit thresh
	#endif
#endif
	
#ifndef STAGE_TIMEOUT_TEST_MODE	
	#define STAGE_TIMEOUT_TEST_MODE			0	///< timeout check between stages
	#if	STAGE_TIMEOUT_TEST_MODE
		#define RECV_CHECK_TIMEOUT_THRESH		100	///< timeout thresh (us)
	#endif
#endif
	
#ifndef REQ_STAGE_TIMESTAMP
	#define REQ_STAGE_TIMESTAMP				0	///< ts for req-rsp delay estimate
	#if REQ_STAGE_TIMESTAMP
		#define RSTS_HP_ONLY					///< check rs_ts only for high-pri
		#define RSTS_SAMPLE
		#ifdef RSTS_SAMPLE
			#define RSTS_SAMPLE_CYCLE			100000	///< trace frequence
		#endif
		#define REQ_STAGE_TIMEOUT_THREASH		100000	///< timeout threash (ns)
	#endif
#endif
	
#ifndef MAINLOOP_TIMESTAMP
	#define MAINLOOP_TIMESTAMP				0	///< ts for mainloop stage delay
	#if MAINLOOP_TIMESTAMP
	//	#define MLTS_SAMPLE				
		#ifdef MLTS_SAMPLE
			#define MLTS_SAMPLE_CYCLE			100000	///< trace frequence
		#else
			#define MAINLOOP_TIMEOUT_THRESH		1000	///< timeout thresh (ns)
		#endif
		#define MLTS_BATCH_THRESH				1	///< don't print in idle loop
	#endif
#endif

#ifndef ACTIVE_DROP_EMULATE
	#define ACTIVE_DROP_EMULATE				0
	#if ACTIVE_DROP_EMULATE
		#define ACTIVE_DROP_RATE				100	///< reciprocal of rate	
//		#define DROP_UNIFORM	///< drop every packet
		#ifndef DROP_UNIFORM	///< only drop typical packets with filter
			#define DFILTER_SYN		///< drop the SYN packet, exclude SYNACK
			#define DFILTER_SYNACK	///< drop SYNACK packet
			#define DFILTER_THIRDACK	///< drop the ACK pkt in TCP handshake
		#endif
	#endif
#endif
/*----------------------------------------------------------------------------*/
// system realization and modules
#ifndef TCP_OPT_TIMESTAMP_ENABLED
	#define TCP_OPT_TIMESTAMP_ENABLED   	0	///< enabled for rtt measure 
#endif
#ifndef TCP_OPT_SACK_ENABLED
	#define TCP_OPT_SACK_ENABLED        	0	///< not implemented 
#endif
#ifndef FLOW_MIGRATION
	#define FLOW_MIGRATION					0
#endif
#ifndef UNSHARED_APP_MODE
	#define UNSHARED_APP_MODE				1	///< at most one app on every core
#endif

#define SOCK_ALLOC_STACK_ONLY 	1
#define SOCK_ALLOC_APP_ONLY		2
#define SOCK_ALLOC_ALL			3
#ifndef SOCK_ALLOC_MAP
	#define SOCK_ALLOC_MAP	SOCK_ALLOC_STACK_ONLY
#endif

#ifndef IF_TX_CHECK
	#define IF_TX_CHECK 1
#endif
#ifndef IF_RX_CHECK
	#define IF_RX_CHECK 0
#endif

#define SINGLE_NIC_PORT
#ifdef SINGLE_NIC_PORT
	#define IFIDX_SINGLE	0
#endif
/******************************************************************************/
/* static macros */
#define MAX_FLOW_PSTACK	MAX_FLOW_NUM/CONFIG.stack_thread	//for every stack thread
#define MAX_FLOW_PCORE	MAX_FLOW_NUM/CONFIG.num_cores	//for every core
/******************************************************************************/
/* macro check */
#if CONTEXT_SWITCH && \
	((defined(PTHREAD_SCHEDULING) && defined(COROUTINE_SCHEDULING)) ||	\
	((!defined(PTHREAD_SCHEDULING) && !defined(COROUTINE_SCHEDULING))))
	#error choose either PTHREAD_SCHEDULING or COROUTINE_SCHEDULING 
#endif

#if (PRIORITY_RECV && !(DRIVER_PRIORITY || NIC_PRIORITY || TCP_PRIORITY || \
			USER_DEFINED_PRIORITY))
	#error choose one priority identification method
#endif
#if (DRIVER_PRIORITY + NIC_PRIORITY + TCP_PRIORITY + USER_DEFINED_PRIORITY > 1)
	#error cheose only one priority identification method
#endif

#if !(defined(MAX_CORE_NUM) && defined(MAX_STACK_NUM) && \
		defined(MAX_SERVER_NUM) && defined(MAX_APP_NUM) && \
		defined(MAX_FLOW_NUM) && defined(MEM_SCALE)) 
	#error missing define of typical macros
#endif
/******************************************************************************/
#ifndef CORE_MAP_FUNC
#define CORE_MAP_FUNC
static inline void
core_map_init(char *core_map)
{
	int i;
	for (i=0; i<MAX_CORE_NUM; i++) {
		core_map[i] = i;
	}
}
#endif
/******************************************************************************/
#endif //#ifdef __GLOBAL_MACRO_H_
