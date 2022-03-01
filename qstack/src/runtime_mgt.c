 /**
 * @file runtime_mgt.c
 * @brief manage the thread scheduling among runtimes
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.1.2
 * @version 1.0
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
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif
#define _GNU_SOURCE
/*----------------------------------------------------------------------------*/
#include "qstack.h"
#include "runtime_mgt.h"
#include <sched.h>
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
static void
app_thread_start(qapp_t qapp)
{
#ifdef PTHREAD_SCHEDULING
	qapp->app_func(qapp->args);
#endif
#ifdef COROUTINE_SCHEDULING
    qCoAttr_t attr;
    attr.stack_size = 8*1024*1024;
	
	// create new use-level stack thread and resume it
	q_coCreate(&qapp->thread, &attr, qapp->app_func, 
			qapp->args);
	TRACE_THREAD("app thread %d created @ Core %d\n", 
			qapp->app_id, qapp->core_id);
	TRACE_THREAD("try to resume to app thread %d @ Core %d!\n",
			qapp->app_id, qapp->core_id);
	q_coResume(qapp->thread);
#endif
}

static int
__create_app_thread(qapp_t qapp, app_func_t app_func, void *args)
{
#ifdef COROUTINE_SCHEDULING
	if (qapp->rt_ctx->rt_thread) {
		// the runtime has already been created by stack thread
	#if CONTEXT_SWITCH
    	qCoAttr_t attr;
    	attr.stack_size = 0;
		qapp->app_func = app_func;
		qapp->args = args;
		q_coCreate(&qapp->thread, &attr, app_func, args);
		TRACE_THREAD("app thread %d created at core %d\n", 
				qapp->app_id, qapp->core_id);
		qapp->rt_ctx->wakeup_ctx.waiting = 1;
		qapp->rt_ctx->wakeup_ctx.pending = 1;
		qapp->rt_ctx->wakeup_ctx.knocked = 1;
		return 0;
	#else
		TRACE_ERR("The application thread and stack thread "
				"should notbe on the same core @ Core!\n", qapp->core_id);
	#endif
	}
#endif
	// if there are not runtime running on the target core, create a new 
	// thread with pthread.
	pthread_attr_t attr;
	cpu_set_t cpus;

	pthread_attr_init(&attr);
	CPU_ZERO(&cpus);    
	CPU_SET(core_map[qapp->rt_ctx->core_id]+CONFIG.core_offset, &cpus);
	pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);

	qapp->app_func = app_func;
	qapp->args = args;
	return pthread_create(&qapp->rt_ctx->rt_thread, &attr, app_thread_start, 
			(void*)qapp);
}

static inline qapp_t
create_app_pthread(int core_id, app_func_t app_func, void *args, int type)
{
	static int app_id = 0;
	static int worker_id = 0;
	int i;
	qcore_t qcore = get_core_context(core_id);
#if UNSHARED_APP_MODE
	if (qcore->rt_ctx->qapp != NULL) {
		TRACE_ERR("There have already been application thread running on "
				"core %d!\n", core_id);
		return NULL;
	}
#endif
	rtmgt_t runtime_mgt = get_runtime_mgt();
    void*test_op = (void *)runtime_mgt;
    
	// qapp init
	qapp_t qapp = (qapp_t)calloc(1, sizeof(struct qapp_context));
	if (type == 0) {
		// it's an app_thread
		qapp->app_id = app_id++;
	} else {
		// it's an worker_thread
		qapp->app_id = MAX_APP_NUM - 1 - (worker_id++);
	}

	qapp->core_id = core_id;

    test_op = (void *)runtime_mgt;
#if 0
	// runtime management
	active_tq_insert(runtime_mgt, core_id);
	for (i=0; i<MAX_STACK_NUM; i++) {
		runtime_mgt->task_map_s[i][core_id] = core_id;
		runtime_mgt->task_map_r[core_id][i] = core_id;
	}

#endif
	qcore->rt_ctx->qapp = qapp;
	qapp->rt_ctx = qcore->rt_ctx;
	get_global_ctx()->app_contexts[qapp->app_id] = qapp;

	TRACE_INFO("Server %d thread start.\n", qapp->app_id);
	if (__create_app_thread(qapp, app_func, args)) {
		perror("pthread_create");
		TRACE_ERR("Failed to create server thread.\n");
		exit(-1);
	}
	return qapp;
}
/******************************************************************************/
/* local functions */
void 
__do_check(rtctx_t rt_ctx, systs_t cur_ts)
{
	io_recv_check(rt_ctx->qstack, 0, cur_ts);
}
/******************************************************************************/
/* functions */
qapp_t
__qstack_create_app(int core_id, app_func_t app_func, void *args)
{
	return create_app_pthread(core_id, app_func, args, 0);
}

qapp_t
__qstack_create_worker(int core_id, app_func_t app_func, void *args)
{
	return create_app_pthread(core_id, app_func, args, 1);
}

void
runtime_init(rtctx_t rt_ctx, int core_id)
{
	rt_ctx->core_id = core_id;
	rt_ctx->qstack = NULL;
	rt_ctx->qapp = NULL;
	rt_ctx->rt_thread = NULL;
	rt_ctx->last_event_check_ts = 0;
}

// Every exit of this function should be carefully checked. Don't regard yieldto 
// as return.
void 
runtime_schedule(rtctx_t rt_ctx, systs_t cur_ts)
{
	if (cur_ts == 0) {
		cur_ts = get_sys_ts();
	}

	if (rt_ctx->qstack && rt_ctx->qapp) {
		// check qstack timeout first
		if (cur_ts-rt_ctx->last_stack_ts>STACK_PENDING_TIMEOUT ) {
			if (rt_ctx->on_stack) {
				TRACE_EXCP("big intervel since last stack check!\n");
			} else {
				yield_to_stack(rt_ctx);
			}
		} else {
			qapp_t qapp = get_active_qapp(rt_ctx);
			if (rt_ctx->on_stack) {
				yield_to_qapp(rt_ctx, rt_ctx->qstack->thread, qapp);
			} else {
				yield_to_stack(rt_ctx);
			}
		}
	}
}

void
yield_to_stack(rtctx_t rt_ctx)
{
	TRACE_THREAD("yield to stack thread @ Core %d\n", rt_ctx->core_id);
	qapp_t qapp = get_cur_qapp(rt_ctx);
	rt_ctx->on_stack = 1;
	q_coYield_to(qapp->thread, rt_ctx->qstack->thread);
}

int 
wakeup_app_thread(qapp_t qapp)
{
	qapp->rt_ctx->wakeup_ctx.knocked = 1;
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
