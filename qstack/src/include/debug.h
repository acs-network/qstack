/**
 * @file debug.h 
 * @brief describe infomations printed by system in different levels
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.7.17
 * @version 1.0
 * @detail Function list: \n
 *   TRACE_API: (INFO) used for api information (not exceptions) \n
 *   TRACE_EPOLL: (DETAIL) detail informations from qepoll framework \n
 *   TRACE_BUFF: (DETAIL) detail informations of rb and sb \n
 *   TRACE_MBUF: (DETAIL) detail informations from mbuf \n
 *   TRACE_STREAM: (DETAIL) detail informations of tcp_stream_t \n
 *   TRACE_OOO: (TRACE) out-of-order occurs \n
 *   TRACE_STATE: (DEBUG) tcp state machine transfer \n
 *   TRACE_CNCT: (TRACE) detail informations on connecting path \n
 *   TRACE_CLOSE: (TRACE) detail informations on closing path \n
 *   TRACE_INIT: (TRACE) detail informations when init the system \n
 *   TRACE_MEMORY: (LOG) memory allocated during the system init \n
 *   TRACE_CHECKP: (DEBUG) called at the beginning of functions or important
 *   			point to check if the code is executed \n
 1   TRACE_EVENT: (DETAIL) very detailed information of every event \n
 *   TRACE_THREAD: (DEBUG) detail information for thread management \n
 *   TRACE_MBUFPOOL: (DEBUG) detail information of mbuf allocation and free \n
 *   TRACE_BYTES: (TRACE) print byte data in mbufs \n
 *   TRACE_SENDQ: (DETAIL) very detailed information of send queue process \n
 *
 *   TRACE_TEMP: (TRACE) temp print for debugging, should be removed after \n
 *   TRACE_LOOP: (DETAIL) at the beginning of loop, will be called many times 
 *   			and severely impact performance \n
 *   TRACE_DBG: (DEBUG) debug informations \n
 *   TRACE_ERROR = TRACE_ERR: (LOG) fatal error and exit(0) \n
 *   TRACE_EXCP: (TRACE) exception lead to unexcepted branch \n
 *   TRACE_TODO: (fprintf) not implemented branches which should not be 
 *   			executed, and exit(0) \n
 *   TRACE_EXIT: (fprintf) exit the program \n
 *   TRACE_MSG: send message to message collecting thread, with little
 *   			overhead at the calling thread \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.6
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2018.7.17
 *   	Author: Shen Yifan
 *   	Modification: add trace levels
 *   3. Date: 2019.3.15
 *   	Author: Shen Yifan
 *   	Modification: rewrite file head, add TRACE_STATE and TRACE_CLOSE
 *   4. Date: 
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __DEBUG_H_
#define __DEBUG_H_
/******************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <sched.h>
/******************************************************************************/
// forward declaration before any Qstack head file
#define USE_MESSAGE				0	// define this before message.h
extern FILE *fp_log;
extern FILE *fp_screen;
/******************************************************************************/
#include "message.h"
#include "basic_clock.h"
//#include "universal.h"
/******************************************************************************/
/* global varialbes */
//#define TRACE_EXCP_AS_ERR		  /* turn TRACE_EXCP to TRACE_ERR */
#define TRACE_FUNC_ON 			1 /* enable the trace print */
#define TRACE_WITH_TS			1 /* add timestamp print to TRACE_DEBUG */
#define TRACE_ERROR_HOOK		1 /* hook function for gdb break point */
#define TRACE_SCREEN_REPLICA	1 /* replicate screen info to file */
//#define LOG_PATH "/mnt/883DCT/shenyifan/log.out"
#define LOG_PATH "./log.out"
#define SCREEN_PATH "./screen.out"
/******************************************************************************/
#define QDBG_OOO		0	/* out-of-order processing path */
#define QDBG_CLOSE		0	/* connection close path */
#define QDBG_CNCT		0	/* connection establish path */
#define QDBG_STATE		0	/* tcp state machine transfer */
#define QDBG_CHECKP		0	/* beginning of functions or important branch */
#define QDBG_TEMP		0	/* very temp print for debugging */
#define QDBG_LOOP		0	/* detail for every loop, severely impact on 
							   performance */
#define QDBG_EVENT		0	/* very detailed information of a certain event */
#define QDBG_EPOLL		0	/* very detailed information of epoll processing */
#define QDBG_BUFF		0	/* very detailed information of a certain rb or 
							   sb */
#define QDBG_MBUF		1	/* very detailed information of a certain mbuf */
#define QDBG_STREAM		0	/* very detailed information of a certain 
							   tcp_stream */
#define QDBG_INIT		0	/* very detailed information when system init */
#define QDBG_MEMORY		0	/* memory allocated during  system init */
#define QDBG_TIMER		0	/* detail information from tcp timer */
#define QDBG_THREAD		0	/* detail information for thread management */
#define QDBG_MBUFPOOL	0	/* information of mbuf allocation and free */
#define QDBG_BYTES		0	/* trace byte data in the mbufs */
#define QDBG_SENDQ		0	/* very detailed information of send queue */
/******************************************************************************/
/* trace level definition */
/** 
 * 0 NONE: do not trace anything 
 */
#define	TRACELV_NONE	0
/** 
 * 1 LOG: only print system logs like crash information 
 */
#define	TRACELV_LOG		1
/** 
 * 2 PROC: proc info like throughput and packet num which was collect and print 
 * periodically for performance monitoring 
 */
#define	TRACELV_PROC	2
/** 
 * 3 TRACE: sensitive trace like out-of-order or retransmition for rera event 
 * monitoring 
 */
#define	TRACELV_TRACE	3
/** 
 * 4 INFO: some info to follow system state like finishing init or context switch 
 * for system logic analyzing 
 */
#define	TRACELV_INFO	4
/** 
 * 5 DEBUG: info for debuging, should not appear when work normally 
 */
#define	TRACELV_DEBUG	5
/** 
 * 6 DETAIL: info for very detailed debuging which may heavily damage performance 
 */
#define	TRACELV_DETAIL	6
/******************************************************************************/
//#define TRACE_LEVEL_UNIVERSAL	TRACELV_DETAIL
//#define TRACE_LEVEL_UNIVERSAL	TRACELV_DEBUG
#define TRACE_LEVEL_UNIVERSAL	TRACELV_TRACE

/* define TRACE_LEVEL at the very beginning in every .c files, otherwise
 * the trace level is equal to the global setting TRACE_LEVEL_UNIVERSAL */
#ifndef TRACE_LEVEL
	#define TRACE_LEVEL TRACE_LEVEL_UNIVERSAL
#endif
/******************************************************************************/
// standard trace functions for different trace levels
#define PRINT_FUNC(f, m...) fprintf(stderr, f, ##m)
#define TRACE_FUNC_TS(f,m...) TRACE_FUNC("[%llu]" f, get_time_ns(), ##m)
#define TRACE_FUNC_MSG_TS(f,m...) TRACE_FUNC_MSG("[%llu]" f, get_time_ns(), ##m)

#if	!TRACE_FUNC_ON || TRACE_LEVEL == TRACELV_NONE
	#define TRACE_FUNC(f, m...) (void)0
	#define TRACE_FILEOUT(f, m...) (void)0
	#define TRACE_SCREEN(f, m...) (void)0
	#define TRACE_FUNC_MSG(f, m...) (void)0
#else
	#define TRACE_FUNC(f, m...) fprintf(stderr, "[%2d]" f, sched_getcpu(), ##m)
	#if USE_MESSAGE
		#define TRACE_FILEOUT(f, m...) do {									\
					int core_id = sched_getcpu();							\
					TRACE_MSG(core_id, "[%2d]" f, core_id, ##m);			\
				} while(0)
		#define TRACE_FUNC_MSG(f, m...) TRACE_FILEOUT(f, ##m)
	#else
		#define TRACE_FILEOUT(f, m...) fprintf(fp_log, f, ##m)
		#define TRACE_FUNC_MSG(f, m...) TRACE_FUNC(f, ##m)
	#endif
	
	#if TRACE_SCREEN_REPLICA
		#define TRACE_SCREEN(f, m...) do {	\
			char buff[4096];	\
			sprintf(buff, f, ##m);	\
			fputs(buff, stdout);	\
			fputs(buff, fp_screen);	\
		} while(0)
	#else
		#define TRACE_SCREEN(f, m...) fprintf(stdout, f, ##m)
	#endif
#endif

#if TRACE_LEVEL >= TRACELV_LOG
	#define TRACE_LOG(f,m...) TRACE_FUNC(f,##m)
#else
	#define TRACE_LOG(f,m...) (void)0
#endif

#if TRACE_LEVEL >= TRACELV_PROC
	#define TRACE_PROC(f,m...) TRACE_FUNC(f,##m)
#else
	#define TRACE_PROC(f,m...) (void)0
#endif

#if TRACE_LEVEL >= TRACELV_TRACE
	#define TRACE_TRACE(f,m...) TRACE_FUNC_MSG_TS(f,##m)
#else
	#define TRACE_TRACE(f,m...) (void)0
#endif

#if TRACE_LEVEL >= TRACELV_INFO
	#define TRACE_INFO(f,m...) TRACE_FUNC_MSG_TS(f,##m)
#else
	#define TRACE_INFO(f,m...) (void)0
#endif

#if TRACE_LEVEL >= TRACELV_DEBUG
	#define TRACE_DEBUG(f,m...) TRACE_FUNC_MSG_TS(f, ##m)
#else
	#define TRACE_DEBUG(f,m...) (void)0
#endif

#if TRACE_LEVEL >= TRACELV_DETAIL
	#define TRACE_DETAIL(f,m...) TRACE_FUNC_MSG_TS(f,##m)
#else
	#define TRACE_DETAIL(f,m...) (void)0
#endif

/******************************************************************************/
// derived trace functions
#define TRACE_API(f,m...) 	TRACE_INFO(f,##m)
#define TRACE_DBG(f,m...) 	TRACE_DEBUG(f,##m)
#define TRACE_ERROR(f,m...) 	TRACE_ERR(f,##m)

/*----------------------------------------------------------------------------*/
#if QDBG_OOO
	#define TRACE_OOO(f,m...)	TRACE_TRACE("[OOO]" f,##m)
#else
	#define TRACE_OOO(f,m...)	(void)0
#endif

#if QDBG_CLOSE
	#define TRACE_CLOSE(f,m...)	TRACE_TRACE("[CLOSING]" f,##m)
#else
	#define TRACE_CLOSE(f,m...)	(void)0
#endif

#if QDBG_CNCT
	#define TRACE_CNCT(f,m...)	TRACE_TRACE("[CONNECT]" f,##m)
#else
	#define TRACE_CNCT(f,m...)	(void)0
#endif

#if QDBG_STATE
	#define TRACE_STATE(f,m...)	TRACE_DEBUG("[STREAM_STATE]"f,##m)
#else
	#define TRACE_STATE(f,m...)	(void)0
#endif

#if QDBG_CHECKP
//	#define TRACE_CHECKP(f,m...) 	TRACE_DEBUG("@@@@[CHECKPOINT][%9llu]" f, \
//											get_time_us(), ##m)
	#define TRACE_CHECKP(f,m...) 	TRACE_DEBUG("[CHECKPOINT]" f, ##m)
#else
	#define TRACE_CHECKP(f,m...)	(void)0
#endif

#if QDBG_TEMP
	#define TRACE_TEMP(f,m...)	TRACE_TRACE("[####TEMP]" f,##m)
#else
	#define TRACE_TEMP(f,m...)	(void)0
#endif

#if QDBG_LOOP
	#define TRACE_LOOP(f,m...) 	TRACE_DETAIL("[~~~~LOOP]"f,##m)
#else
	#define TRACE_LOOP(f,m...)	(void)0
#endif

#if QDBG_EVENT
	#define TRACE_EVENT(f,m...)	TRACE_DETAIL("[EVENT]" f,##m)
#else
	#define TRACE_EVENT(f,m...)	(void)0
#endif

#if QDBG_THREAD
	#define TRACE_THREAD(f,m...)	TRACE_DEBUG("[THREAD]" f,##m)
#else
	#define TRACE_THREAD(f,m...)	(void)0
#endif

#if QDBG_EPOLL
	#define TRACE_EPOLL(f,m...)	TRACE_DETAIL("[EPOLL]" f,##m)
#else
	#define TRACE_EPOLL(f,m...)	(void)0
#endif

#if QDBG_BUFF
	#define TRACE_BUFF(f,m...) 	TRACE_DETAIL("[BUFF]" f, ##m)
#else
	#define TRACE_BUFF(f,m...)	(void)0
#endif

#if QDBG_MBUF
	#define TRACE_MBUF(f,m...) 	TRACE_DEBUG("[MBUF]" f, ##m)
#else
	#define TRACE_MBUF(f,m...)	(void)0
#endif

#if QDBG_STREAM
	#define TRACE_STREAM(f,m...) 	TRACE_DETAIL("[STREAM]" f, ##m)
#else
	#define TRACE_STREAM(f,m...)	(void)0
#endif

#if QDBG_INIT
	#define TRACE_INIT(f,m...) 	TRACE_TRACE("[INIT]" f, ##m)
#else
	#define TRACE_INIT(f,m...)	(void)0
#endif

#if QDBG_TIMER
	#define TRACE_TIMER(f,m...) 	TRACE_DEBUG("[TIMER]" f, ##m)
#else
	#define TRACE_TIMER(f,m...)	(void)0
#endif

#if QDBG_MEMORY
	#define TRACE_MEMORY(f,m...) 	TRACE_LOG("[MEMORY]" f, ##m)
#else
	#define TRACE_MEMORY(f,m...)	(void)0
#endif

#if QDBG_MBUFPOOL
	#define TRACE_MBUFPOOL(f,m...) 	TRACE_DEBUG("[MBUFPOOL]" f, ##m)
#else
	#define TRACE_MBUFPOOL(f,m...)	(void)0
#endif

#if QDBG_BYTES
	#define TRACE_BYTES(f,m...) 	TRACE_LOG("[BYTES]" f, ##m)
	#define TRACE_BYTES_X(f,m...) 	PRINT_FUNC(f, ##m)
#else
	#define TRACE_BYTES(f,m...) (void)0
	#define TRACE_BYTES_X(f,m...) (void)0
#endif

#if QDBG_SENDQ
	#define TRACE_SENDQ(f,m...) 	TRACE_TRACE(f, ##m)
#else
	#define TRACE_SENDQ(f,m...) (void)0
#endif
/*----------------------------------------------------------------------------*/
#define TRACE_ERR(f,m...) 	do {	\
			fprintf(stderr, "[%2d][%llu][ERR]@[%10s:%4d] " f,	\
					sched_getcpu(), get_time_ns(), __FILE__, __LINE__, ##m); \
			immediately_print_netwrok_state();	\
			hook_error();	\
			exit(0);	\
		} while(0)
#define TRACE_TODO(f,m...) 	do {	\
			fprintf(stderr, "[%2d][%10s:%4d] not implemented!\n" f,	\
					sched_getcpu(), __FILE__, __LINE__, ##m);	\
			immediately_print_netwrok_state();	\
			exit(0);	\
		} while (0)
#define TRACE_EXIT(f,m...) 	do {	\
			fprintf(stderr, "[%2d][EXIT]@[%10s:%4d]" f,	\
					sched_getcpu(), __FILE__, __LINE__, ##m);	\
			immediately_print_netwrok_state();	\
			exit(0);	\
		} while (0)

//#define TRACE_MSG(c,f,m...)	(void)0
#define TRACE_MSG(c, f, ...)	do {	\
			msg_t msg = msgq_get_wptr(c);	\
			encode_message(msg, (f), __VA_ARGS__);	\
			msgq_send_msg(c);	\
		} while(0)
/*----------------------------------------------------------------------------*/
#ifndef TRACE_EXCP_AS_ERR
	#define TRACE_EXCP(f,m...) 	fprintf(stderr, "[%2d][%llu][EXCP]@[%10s:%4d] " \
					f, sched_getcpu(), get_time_ns(), __FILE__, __LINE__, ##m)
#else
	#define TRACE_EXCP(f,m...) 	TRACE_ERR("[EXCP->ERR]" f, ##m)
#endif
/******************************************************************************/
static inline void
hook_error()
{
#if TRACE_ERROR_HOOK
	fprintf(stderr, "this function is used for gdb debugging");
#endif
}
/******************************************************************************/
#endif //#ifndef __DEBUG_H_
