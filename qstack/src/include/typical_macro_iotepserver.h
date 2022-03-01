/**
 * @file typical_macro_iotepserver.h
 * @brief typical macros for the iotepserver
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
#ifndef __TYPICAL_MACRO_IOTEPSERVER_H_
#define __TYPICAL_MACRO_IOTEPSERVER_H_
/******************************************************************************/
/* global macros */
// these should only be used at array static allocation
#define MAX_CORE_NUM 	24
#define MAX_STACK_NUM 	4
#define MAX_SERVER_NUM 	20
#define MAX_APP_NUM			MAX_CORE_NUM
#define MONITOR_THREAD_CORE	7	///< dedicated core for monitor

#define MAX_FLOW_NUM	14000000
#define MEM_SCALE		6		///< use less memory of 2^MEM_SCALE
/******************************************************************************/
#define STATISTIC_STATE_DETAIL			1	///< detail statistic
#define EXTRA_RECV_CHECK		        2	
/* number of workers (redis-server) per server thread */
#define WORKER_PER_SERVER 				4	
#define STATISTIC_FORWARD_BUFF_LEN		
#define IOTEPSERVER_SETTINGS
/******************************************************************************/
#ifndef CORE_MAP_FUNC
#define CORE_MAP_FUNC
	#if 1 // for 6130 
		#define PHYSOCK_NUM		2
		#define CORE_PER_SOCK	16
static inline void
core_map_init(char *core_map)
{
	//0, 8, 16, 24, 1, 9, 17, 25, 2, 3, 10, 11, 18, 19, 26, 27
	int i, j, group, idx, sock;
	int stack_per_sock = (MAX_STACK_NUM-1) / PHYSOCK_NUM + 1; // round up
	int group_interval = CORE_PER_SOCK / stack_per_sock; // core group interval

	char redis_core[MAX_STACK_NUM * WORKER_PER_SERVER] = {0};
	char redis_hyper[MAX_STACK_NUM * WORKER_PER_SERVER] = {0};

	for (sock = 0; sock<PHYSOCK_NUM; sock++) {
		for (i=0; i<MAX_STACK_NUM / PHYSOCK_NUM; i++) {
			group = sock*MAX_STACK_NUM/PHYSOCK_NUM + i;
			fprintf(stderr, "===================\ncore group %d:\n", group);
			// one stack thread per group
			idx = group;
			core_map[idx] = sock*CORE_PER_SOCK + i*group_interval;	// stack threads
			fprintf(stderr, "stack thread: logic %d on physic %d\n", 
					idx, core_map[idx]);

			idx = MAX_STACK_NUM+group;
			core_map[idx] = core_map[group] + 1;	// server threads
			fprintf(stderr, "server thread: logic %d on physic %d\n", 
					idx, core_map[idx]);

			for (j=0; j<WORKER_PER_SERVER; j++) {
				// worker threads
				idx = 2*MAX_STACK_NUM + group*WORKER_PER_SERVER + j;
				core_map[idx] = core_map[group] + 2 + j;
				fprintf(stderr, "worker thread: logic %d on physic %d\n", 
						idx, core_map[idx]);
				redis_hyper[group*WORKER_PER_SERVER+j] = core_map[idx];
				redis_core[group*WORKER_PER_SERVER+j] = core_map[group] + 2 
						+ WORKER_PER_SERVER + j;
			}
		}
	}
	fprintf(stderr, "====================\n"
			"suggest redis-server bind without hyper-thread:\n");
	for (i=0; i<MAX_STACK_NUM * WORKER_PER_SERVER; i++) {
		fprintf(stderr, "%d ", redis_core[i]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "or bind redis-server to sibling core of "
			"following cores with hyper-thread:\n");
	for (i=0; i<MAX_STACK_NUM * WORKER_PER_SERVER; i++) {
		fprintf(stderr, "%d ", redis_hyper[i]);
	}
	fprintf(stderr, "\n");
}
	#endif
	#if 0	// for 7285
		#define PHYSOCK_NUM		2
		#define CORE_PER_SOCK	32
static inline void
core_map_init(char *core_map)
{
	int group_interval = 8;		// core group interval, related to CPU arch
	int i, j, sock, group, idx;
	char redis_core[MAX_STACK_NUM * WORKER_PER_SERVER] = {0};
	for (sock = 0; sock<PHYSOCK_NUM; sock++)
		for (i=0; i<MAX_STACK_NUM / PHYSOCK_NUM; i++) {
			group = sock*MAX_STACK_NUM/PHYSOCK_NUM + i;
			fprintf(stderr, "===================\ncore group %d:\n", group);
			// one stack thread per group
			idx = group;
			core_map[idx] = i*2 + sock*CORE_PER_SOCK;	// stack threads
			fprintf(stderr, "stack thread: logic %d on physic %d\n", 
					idx, core_map[idx]);
	
			idx = MAX_STACK_NUM+i;
			core_map[idx] = core_map[group] + 1;	// server threads
			fprintf(stderr, "server thread: logic %d on physic %d\n", 
					idx, core_map[idx]);
	
			for (j=0; j<WORKER_PER_SERVER; j++) {
				// worker threads
				idx = 2*MAX_STACK_NUM + i*WORKER_PER_SERVER + j;
				core_map[idx] = (group+1) * group_interval + 2*j;
				fprintf(stderr, "worker thread: logic %d on physic %d\n", 
						idx, core_map[idx]);
				redis_core[group*WORKER_PER_SERVER+j] = core_map[idx] + 1;
			}
		}
	fprintf(stderr, "====================\nsuggest redis-server bind:\n");
	for (i=0; i<MAX_STACK_NUM * WORKER_PER_SERVER; i++) {
		fprintf(stderr, "%d ", redis_core[i]);
	}
	fprintf(stderr, "\n");
	exit(0);
}
	#endif
#else
	#error only one core_map_init() is available!
#endif
/******************************************************************************/
/**
 * 
 *
 * @param 
 * @param[out]
 * @return
 * 	
 * @ref 
 * @see
 * @note
 */
#endif //#ifdef __TYPICAL_MACRO_IOTEPSERVER_H_
