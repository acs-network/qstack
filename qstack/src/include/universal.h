/**
 * @file universal.h
 * @brief universal macros and header files
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.3.1
 * @version 1.0
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/** - History:
 *   1. Date: 2018.6.8
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2018.6.12
 *   	Author: Shen Yifan
 *   	Modification: add templated comments
 *   3. Date: 2019.3.1
 *   	Author: Shen Yifan
 *   	Modification: change file head comments, remove time functions to 
 *   					basic_clock.h
 *   4. Date:
 *   	Author:
 *   	Modification:
 */
#ifndef __UNIVERSAL_H_
#define __UNIVERSAL_H_
/******************************************************************************/
#include <sys/queue.h>	// /usr/include/sys/queue.h

#include "global_macro.h"
#include "debug.h"
/******************************************************************************/
/* macro functions */
// to avoid vars undeclaration in qstack_t if turn off the statistic mode
#if STATISTIC_STATE_BASIC
	#define BSTAT_ADD(a,b)	STAT_ADD(a,b)
	#define BSTAT_SET(a,b)  ((a)=(b))
	#define BSTAT_CHECK_ADD(a,b,c)	STAT_CHECK_ADD(a,b,c)
	#define BSTAT_CHECK_SET(a,b,c)	STAT_CHECK_SET(a,b,c)
#else
	#define BSTAT_ADD(a,b)	(void)0
	#define BSTAT_SET(a,b)  (void)0
	#define BSTAT_CHECK_ADD(a,b,c)	(void)0
	#define BSTAT_CHECK_SET(a,b,c)	(void)0
#endif

#if STATISTIC_STATE_DETAIL
	#define DSTAT_ADD(a,b)	BSTAT_ADD(a,b)
	#define DSTAT_SET(a,b)	BSTAT_SET(a,b)
	#define DSTAT_CHECK_ADD(a,b,c)	BSTAT_CHECK_ADD(a,b,c)
	#define DSTAT_CHECK_SET(a,b,c)	BSTAT_CHECK_SET(a,b,c)
#else
	#define DSTAT_ADD(a,b)	(void)0
	#define DSTAT_SET(a,b)	(void)0
	#define DSTAT_CHECK_ADD(a,b,c)	(void)0
	#define DSTAT_CHECK_SET(a,b,c)	(void)0
#endif

#if STATISTIC_TEMP
	#define TSTAT_ADD(id,core,a) STAT_ADD(q_stat.stat_temp[id][core],a)
#else
	#define TSTAT_ADD(id,core,a) (void)0
#endif

#if STATISTIC_STATE							
	#define STAT_ADD(a,b)	((a)+=(b))
	#define STAT_SET(a,b)  ((a)=(b))
	#define STAT_CHECK_ADD(a,b,c)	do { 	\
		if (a) 								\
			STAT_ADD(b,c);					\
		} while (0)	
	#define STAT_CHECK_SET(a,b,c)	do { 	\
		if (a) 								\
			STAT_SET(b,c);					\
		} while (0)	
#else
	#define STAT_ADD(a,b)	(void)0
	#define STAT_ZERO(a,b)	(void)0
	#define STAT_SET(a,b)  (void)0
	#define STAT_CHECK_ADD(a,b,c)	(void)0
	#define STAT_CHECK_SET(a,b,c)	(void)0
#endif
/******************************************************************************/
/* global static variables */
struct q_statistic
{
#if STATISTIC_TEMP
	volatile uint32_t stat_temp[STAT_TEMP_NUM][MAX_CORE_NUM];
#endif
};

extern struct q_statistic q_stat;
/******************************************************************************/
/* basic inline functions */
/******************************************************************************/
/* head files with dependent macros */ 
void immediately_print_netwrok_state();
/******************************************************************************/
#endif //ifndef __UNIVERSAL_H_
