/**
 * @file runtime_mgt.h
 * @brief manage the thread scheduling among runtimes
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.1.2
 * @version 1.0
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.1.2
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __RUNTIME_MGT_H_
#define __RUNTIME_MGT_H_
/******************************************************************************/
#include <pthread.h>
//#include "context.h"
#include "routine.h"
/******************************************************************************/
/* global forward declarations */
#ifdef PTHREAD_SCHEDULING
typedef pthread_t qthread_t;
#else
typedef qCoroutine_t *qthread_t;
#endif
typedef int (* app_func_t)(void *);
struct runtime_management;
typedef struct runtime_management *rtmgt_t;
struct runtime_ctx;
typedef struct runtime_ctx *rtctx_t;
/******************************************************************************/
#include "qstack.h"
/******************************************************************************/
/* global macros */
#define MAX_RUNTIME_NUM		MAX_CORE_NUM
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* data structures */
// if the app thread should be wake up
struct thread_wakeup
{
#if CONTEXT_SWITCH
	volatile uint8_t pending;	// whether the app thread is pending
	volatile uint8_t knocked;	// there are requests raised from stack thread
	volatile uint8_t waiting;	// the app thread is waiting to be processed

	#ifdef PTHREAD_SCHEDULING
	pthread_cond_t epoll_cond;
	pthread_mutex_t epoll_lock;
	#endif
#endif
};

struct qapp_context
{
	uint8_t app_id;			///< unique id for every app context instance
	uint8_t core_id;
	qthread_t thread;		///< handle of this app thread
	rtctx_t rt_ctx;

	app_func_t app_func;	///< application thread entry funtion
	void *args;				///< args for app_function
};

struct runtime_ctx
{
	uint8_t on_stack:1;	// whether it's stack thread context
	uint8_t core_id:7;
#if CONTEXT_SWITCH
	struct thread_wakeup wakeup_ctx;
#endif
	systs_t last_stack_ts;	///< last time the stack thread runs main loop
	systs_t last_check_ts;	///< last time check the rx_ring
	systs_t last_event_check_ts;	///< last time get a new event to process
	qstack_t qstack;
	qapp_t qapp;
	pthread_t rt_thread;
};

struct qcore_context
{
	uint8_t core_id;
	rtctx_t rt_ctx;
#ifdef PREEMPTIVE_MODE
	pthread_t control_thread;
#endif
};

struct runtime_management
{
	uint8_t active_tq_num;
	uint8_t active_tq[MAX_RUNTIME_NUM]; // sorted active task queues on cores
	uint8_t task_map_s[MAX_STACK_NUM][MAX_RUNTIME_NUM]; // which tq can send to
	uint8_t task_map_r[MAX_RUNTIME_NUM][MAX_STACK_NUM]; // which tq should check
};
/******************************************************************************/
/* local functions */
void 
__do_check(rtctx_t rt_ctx, systs_t cur_ts);
/******************************************************************************/
/* global inline functions */
// it's actually a single step in a simple array-based insert sort
static inline void
active_tq_insert(rtmgt_t runtime_mgt, uint8_t new_tq)
{
	int i;
	int seq = 0;
	uint8_t *q = runtime_mgt->active_tq;
	uint8_t length = runtime_mgt->active_tq_num;
	if (unlikely(length == MAX_RUNTIME_NUM)) {
		TRACE_ERR("Failed to add new task_queue, "
				"the active task queue is full!\n");
		return;
	}
	while (seq < length && q[seq] < new_tq) {
		// find the target slot of the target item, the items before it are 
		// smaller than it
		seq++;
	}
	if (length == seq) {
		// insert the new task_queue to the tail of active_tq
		q[seq] = new_tq;
	} else {
		// move every item after the target to the slot behind itself
		for (i= length; i>seq; i--) {
			q[i] = q[i-1];
		}
		// then insert the target item to the target slot
		q[seq] = new_tq;
	}
	// increase the array size
	runtime_mgt->active_tq_num++;
}

static inline qapp_t 
get_active_qapp(rtctx_t rt_ctx)
{
	return rt_ctx->qapp;
}


static inline qapp_t 
get_cur_qapp(rtctx_t rt_ctx)
{
	return rt_ctx->qapp;
}

static inline void
yield_to_qapp(rtctx_t rt_ctx, qthread_t from, qapp_t target)
{
	TRACE_THREAD("yield to app thread @ Core %d\n", rt_ctx->core_id);
	rt_ctx->on_stack = 0;
	q_coYield_to(from, target->thread);
}

static inline void
rt_check(rtctx_t rt_ctx, systs_t cur_ts)
{
#ifdef CHECK_INSERT
	if (rt_ctx->qstack) {
		if (cur_ts == FETCH_NEW_TS) {
			cur_ts = get_time_us();
		}
		if (cur_ts - rt_ctx->last_stack_ts > STACK_PENDING_TIMEOUT) {
//			runtime_schedule(rt_ctx, cur_ts);
			yield_to_stack(rt_ctx);
		}
		if (cur_ts - rt_ctx->last_check_ts > STACK_CHECK_TIMEOUT) {
			__do_check(rt_ctx, cur_ts);
		}
	}
	rt_ctx->last_check_ts = cur_ts;
#endif
}
/******************************************************************************/
/* function declarations */
/**
 * create an application thread, and pin it to the target core
 *
 * @param core_id		the core on which the application is goning to run
 * @param app_func		the entry function of application
 * @param args			the args for app_func
 *
 * @return
 * 	return the created application context handle if success;
 * 	otherwise return NULL
 */
qapp_t
__qstack_create_app(int core_id, app_func_t app_func, void *args); 

/**
 * create a worker thread, and pin it to the target core
 *
 * @param core_id		the core on which the application is goning to run
 * @param app_func		the entry function of application
 * @param args			the args for app_func
 *
 * @return
 * 	return the created application context handle if success;
 * 	otherwise return NULL
 */
qapp_t
__qstack_create_worker(int core_id, app_func_t app_func, void *args); 

void
runtime_init(rtctx_t rt_ctx, int core_id);

void 
runtime_schedule(rtctx_t rt_ctx, systs_t cur_ts);

void
yield_to_stack(rtctx_t rt_ctx);

int 
wakeup_app_thread(qapp_t qapp); 
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef __RUNTIME_MGT_H_
