 /**
 * @file basic_clock.c
 * @brief basic clock functions, especialy for init_system_ts()
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.4.10
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.4.10
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
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#define __USE_GNU
#include <sched.h>
#include <pthread.h>

#include "basic_clock.h"
/******************************************************************************/
#ifdef FAST_GLOBAL_CLOCK
volatile ts_t g_fast_clock;
#endif
uint64_t rdtsc_hz;
/******************************************************************************/
/* local static functions */
#ifdef FAST_GLOBAL_CLOCK
void
gfclock_update()
{
	while (1) {
		__q_get_time(g_fast_clock);
	}
}
#endif
/******************************************************************************/
/* functions */
void
init_system_ts()
{
	if (!ts_system_init) {
		rdtsc_hz = rte_get_tsc_hz();
#ifdef FAST_GLOBAL_CLOCK
//		if (GCLOCK_UPDATE_CORE <= MAX_CORE_NUM) {
//			TRACE_ERR("global_clock update thread "
//					"is running on a working core!\n");
//		}
		g_fast_clock = (ts_t)calloc(1, sizeof(struct timespec));
		__q_get_time(g_fast_clock);
    	
		pthread_t c_thread;
		pthread_attr_t attr;
    	cpu_set_t cpus;
    	pthread_attr_init(&attr);
		CPU_ZERO(&cpus);    
        CPU_SET(GCLOCK_UPDATE_CORE , &cpus);     
        pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus);	
		pthread_create(&c_thread, &attr, gfclock_update, NULL);
#endif
		ts_system_init = (ts_t)calloc(1, sizeof(struct timespec));
		__q_get_time(ts_system_init);
	}
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
