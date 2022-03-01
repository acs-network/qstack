/**
 * @file typical_macro_nginx.h
 * @brief typical macros for nginx
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.5.12
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.5.12
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TYPICAL_MACRO_NGINX_H_
#define __TYPICAL_MACRO_NGINX_H_
/******************************************************************************/
/* global macros */
// these should only be used at array static allocation
#define MAX_CORE_NUM 	2
#define MAX_STACK_NUM 	1
#define MAX_SERVER_NUM 	1
#define MAX_APP_NUM			MAX_CORE_NUM

#define MONITOR_THREAD_CORE	MAX_CORE_NUM+2	///< dedicated core for monitor

#define MAX_FLOW_NUM	500000
#define MEM_SCALE		4		///< use less memory of 2^MEM_SCALE
/******************************************************************************/
//#define SHARED_NOTHING_MODE			
#define STATISTIC_STATE_DETAIL			1	///< detail statistic
/******************************************************************************/
#endif //#ifdef __TYPICAL_MACRO_NGINX_H_
