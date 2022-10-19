/**
 * @file basic_clock.h
 * @brief basic time functions for the whole system
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.3.1
 * @version 1.0
 * @detail Function list: \n
 *   1. init_system_ts(): init the global time system\n
 *   2. get_time_ns(): get global system time in ns\n
 *   3. get_time_us(): get global system time in us\n
 *   4. get_time_ms(): get global system time in ms\n
 *   5. get_time_s(): get global system time in s\n
 *   6. get_abs_time_ns(): get absolute time of every core in ns\n
 *   7. q_get_time(): get global absolute time of system
 */
/*----------------------------------------------------------------------------*/
/* - History: 
 *   1. Date: 2019.3.1
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __BASIC_CLOCK_H_
#define __BASIC_CLOCK_H_
/******************************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <rte_cycles.h>
/******************************************************************************/
/* global macros */
//#define FAST_GLOBAL_CLOCK // do not use it, it's not efficient
//#define DPDK_RTE_RDTSC_CLOCK	// it will make global close inaccurate
#ifdef FAST_GLOBAL_CLOCK
	#define GCLOCK_UPDATE_CORE		9
#endif

#define HZ						1000
#define FETCH_NEW_TS	0
/******************************************************************************/
// macro functions
#define TIME_TICK				(1000000/HZ)		// in us
#define TIMEVAL_TO_TS(t)		(uint32_t)((t)->tv_sec * HZ + \
								((t)->tv_usec / TIME_TICK))

#define TS_TO_USEC(t)			((t) * TIME_TICK)
#define TS_TO_MSEC(t)			(TS_TO_USEC(t) / 1000)

#define USEC_TO_TS(t)			((t) / TIME_TICK)
#define MSEC_TO_TS(t)			(USEC_TO_TS((t) * 1000))
#define SEC_TO_TS(t)			(t * HZ)

#define SEC_TO_USEC(t)			((t) * 1000000)
#define SEC_TO_MSEC(t)			((t) * 1000)
#define MSEC_TO_USEC(t)			((t) * 1000)
#define USEC_TO_SEC(t)			((t) / 1000000)
/******************************************************************************/
/* forward declarations */
#ifdef DPDK_RTE_RDTSC_CLOCK
typedef uint64_t basic_ts;
#else
typedef struct timespec basic_ts;
#endif
typedef uint32_t systs_t;

typedef basic_ts *ts_t;
extern ts_t ts_system_init;
#ifdef FAST_GLOBAL_CLOCK
extern volatile ts_t g_fast_clock;
#endif
extern uint64_t rdtsc_hz;
/******************************************************************************/
/* function declarations */
static inline uint64_t
__q_rdtsc()
{
	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));
	return tsc.tsc_64;
}

static inline uint64_t
get_abs_time_ns()
{
	if (unlikely(!rdtsc_hz)) {
		rdtsc_hz = rte_get_tsc_hz();
	}
	return __q_rdtsc() * (uint64_t) 1000/ (rdtsc_hz/1000000);
}

static inline void
__q_get_time(ts_t ts)
{
#ifdef DPDK_RTE_RDTSC_CLOCK
	*ts = rte_rdtsc();
#else
	clock_gettime(CLOCK_MONOTONIC, ts);
#endif
}

static inline void
q_get_time(ts_t ts)
{
#ifdef FAST_GLOBAL_CLOCK
	*ts = *g_fast_clock;
#else
	__q_get_time(ts);
#endif
}

static inline uint64_t 
timeval_ns(ts_t ts1, ts_t ts2)
{
#ifdef DPDK_RTE_RDTSC_CLOCK
	// (*ts2 - *ts1) is around 1e10-1e12
	// rte_get_tsc_hz is 2.2G
	uint64_t ret = (*ts2 - *ts1) * (uint64_t)1000 / 
		(((uint32_t)rte_get_tsc_hz())/1000000);
	return ret;
#else
	return (ts2->tv_sec-ts1->tv_sec)*1000000000 + ts2->tv_nsec-ts1->tv_nsec;
#endif
}

static inline uint64_t 
timeval_us(ts_t ts1, ts_t ts2)
{
	return timeval_ns(ts1, ts2)/1000;
}

static inline uint64_t 
timeval_ms(ts_t ts1, ts_t ts2)
{
	return timeval_ns(ts1, ts2)/1000000;
}

static inline uint64_t 
timeval_s(ts_t ts1, ts_t ts2)
{
	return timeval_ns(ts1, ts2)/1000000000;
}
/******************************************************************************/
/**
 * init the basic system_init_time
 *
 * @return null
 * @note
 * 	should be called at the beginning of system initialization
 */
void
init_system_ts();

/**
 * get time interval (in second) since system init
 *
 * @return 
 * 	tiem interval since system init
 */
static inline uint64_t 
get_time_s()
{
	basic_ts cur_ts;
	q_get_time(&cur_ts);
	return timeval_s(ts_system_init, &cur_ts);
}

/**
 * get time interval (in nanosecond) since system init
 *
 * @return 
 * 	tiem interval since system init
 */
static inline uint64_t
get_time_ns()
{
	basic_ts cur_ts;
	q_get_time(&cur_ts);
	return timeval_ns(ts_system_init, &cur_ts);
}

/**
 * get time interval (in microsecond) since system init
 *
 * @return 
 * 	tiem interval since system init
 */
static inline uint64_t
get_time_us()
{
	basic_ts cur_ts;
	q_get_time(&cur_ts);
	return timeval_us(ts_system_init, &cur_ts);
}

/**
 * get time interval (in millisecond) since system init
 *
 * @return 
 * 	tiem interval since system init
 */
static inline uint64_t
get_time_ms()
{
	basic_ts cur_ts;
	q_get_time(&cur_ts);
	return timeval_ms(ts_system_init, &cur_ts);
}

static inline systs_t
get_sys_ts()
{
	return (systs_t)get_time_ms();
}
/******************************************************************************/
#endif //#ifdef __BASIC_CLOCK_H_
