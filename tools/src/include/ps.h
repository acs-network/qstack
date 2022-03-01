/**
 * @file ps.h
 * @brief global macros and some functions
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.7.22
 * @version 1.0 \n
 * @detail Function list: \n
 *   1. q_prefetch0(): prefetch the target object to all cache level\n
 *   2. q_prefetch2(): prefetch the target objecdt to LLC\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.22
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
#ifndef __PS_H_
#define __PS_H_
/******************************************************************************/
//#include "atomic.h"
#include <stdarg.h>
#include <pthread.h>  
#include <sched.h>
/******************************************************************************/
/* global macros */
#ifndef ETH_ALEN
	#define ETH_ALEN 6
#endif
/******************************************************************************/
#define __Q_CACHE_ALIGNED __attribute__((__aligned__(64)))
//#define __Q_CACHE_ALIGNED
/*----------------------------------------------------------------------------*/
#ifndef TRUE
	#define TRUE (1)
#endif

#ifndef FALSE
	#define FALSE (0)
#endif

#ifndef SUCCESS
	#define SUCCESS (1)
#endif

#ifndef FAILED	// keep aware of the difference between FAILED and ERROR
	#define FAILED (-2)
#endif

#ifndef ERROR	// keep aware of the difference between FAILED and ERROR
	#define ERROR (-1)
#endif
/******************************************************************************/
// functional macros
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
/*----------------------------------------------------------------------------*/
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))
/*----------------------------------------------------------------------------*/
#define Q_ARG_COUNT(...) DO_ARG_COUNT(0, ##__VA_ARGS__,\
			64, 63, 62, 61, 60, \
			59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
			49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
			39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
			29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
			19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
			 9,  8,  7,  6,  5,  4,  3,  2,  1,  0)
#define DO_ARG_COUNT(\
			 _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, \
			_10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
			_20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
			_30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
			_40, _41, _42, _43, _44, _45, _46, _47, _48, _49, \
			_50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
			_60, _61, _62, _63, _64, N, ...) N

#define DO_PARSE_ARGS_FULL(arg_size, argv, \
		_0, _1, _2, _3, _4, _5,	_6, _7, _8, _9, ...) do {	\
		arg_size[0] = sizeof(_0); argv[0] = (uint64_t)_0;	\
		arg_size[1] = sizeof(_1); argv[1] = (uint64_t)_1;	\
		arg_size[2] = sizeof(_2); argv[2] = (uint64_t)_2;	\
		arg_size[3] = sizeof(_3); argv[3] = (uint64_t)_3;	\
		arg_size[4] = sizeof(_4); argv[4] = (uint64_t)_4;	\
		arg_size[5] = sizeof(_5); argv[5] = (uint64_t)_5;	\
		arg_size[6] = sizeof(_6); argv[6] = (uint64_t)_6;	\
		arg_size[7] = sizeof(_7); argv[7] = (uint64_t)_7;	\
		arg_size[8] = sizeof(_8); argv[8] = (uint64_t)_8;	\
		arg_size[9] = sizeof(_9); argv[9] = (uint64_t)_9;	\
	} while(0)

/**
 * parse the args with uncertain length
 *
 * @param arg_size		array of uint8_t, store the length of arguments
 * @param argv			array of uint64_t, store the arguments with uint64_t
 * @param ...			arguments to parse
 *
 * @return null
 *
 * @note
 * 	there should be no more than 10 arguments 
 */
#define PARSE_ARGS_FULL(arg_size, argv, args...) DO_PARSE_ARGS_FULL(\
		arg_size, argv, ##args,	\
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9)

#define DO_PARSE_ARGS(argv, \
		_0, _1, _2, _3, _4, _5,	_6, _7, _8, _9, _10, _11, ...)  do { \
		(argv)[0] = (uint64_t)(_0);		(argv)[1] = (uint64_t)(_1);	\
		(argv)[2] = (uint64_t)(_2);		(argv)[3] = (uint64_t)(_3);	\
		(argv)[4] = (uint64_t)(_4);		(argv)[5] = (uint64_t)(_5);	\
		(argv)[6] = (uint64_t)(_6);		(argv)[7] = (uint64_t)(_7);	\
		(argv)[8] = (uint64_t)(_8);		(argv)[9] = (uint64_t)(_9);	\
		(argv)[10] = (uint64_t)(_10);	(argv)[11] = (uint64_t)(_11);	\
	} while(0)

/**
 * parse the args with uncertain length
 *
 * @param argv			array of uint64_t, store the arguments with uint64_t
 * @param ...			arguments to parse
 *
 * @return null
 *
 * @note
 * 	there should be no more than 12 arguments 
 */
#define PARSE_ARGS(argv, args...) DO_PARSE_ARGS(argv, ##args, \
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)
/******************************************************************************/
__extension__ typedef void *MARKER[0];
__extension__ typedef uint8_t MARKER8[0];
/******************************************************************************/
/* functions declaration */
static inline void 
q_prefetch0(const volatile void *p)
{
	if (p) {
		asm volatile ("prefetcht0 %[p]" : : [p] "m" 
				(*(const volatile char *)p));
//		__builtin_prefetch(p);
	}
}

static inline void 
q_prefetch2(const volatile void *p)
{
	if (p) {
		asm volatile ("prefetcht2 %[p]" : : [p] "m" 
				(*(const volatile char *)p));
	}
}

static inline int
q_get_core_id()
{
	return sched_getcpu();
}
/*----------------------------------------------------------------------------*/
struct myva_list
{
	unsigned int gp_offset;
	unsigned int fp_offset;
	void *overflow_arg_area;
	void *reg_save_area;
};

/**
 * parse the args with uncertain length
 *
 * @param buffer		the buffer to store the arguments
 * @param argc			the number of arguments to parse
 * @param ...			arguments to parse
 *
 * @return null
 *
 * @note
 * 	notice the length limitation of buffer 
 */
static void
parse_args_va(char *buffer, int argc, ...) __attribute__((noinline));
/******************************************************************************/
static void
parse_args_va(char *buffer, int argc, ...)
{
	va_list ap;
	va_start(ap, argc);
	int reg_num = (48 - ap->gp_offset) / 8;
	if (argc <= reg_num) {
		memcpy(buffer, ((char*)ap->reg_save_area) + ap->gp_offset, argc*8);
	} else {
		memcpy(buffer, ((char*)ap->reg_save_area) + ap->gp_offset, reg_num*8);
		memcpy(buffer+reg_num*8, ap->overflow_arg_area, (argc-reg_num)*8);
	}
}
/******************************************************************************/
// pthread functions
#define qps_create_pthread(thread, core_id, func, arg)	do { \
    pthread_attr_t attr; \
    cpu_set_t cpus; \
	pthread_attr_init(&attr); \
    CPU_ZERO(&cpus); \
	CPU_SET((core_id), &cpus); \
    pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpus); \
    pthread_create((thread), &attr, (func), (arg)); \
	} while(0)
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef ___H_
