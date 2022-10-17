

/*******************************************************************************
 * - File name: dpdk_chunk.h 
 * - Author: zhangzhao (mail:zhangzhao@ict.ac.cn) 
 * - Date: 2018.9.4
 * - Description: functions for chunk
 * - Version: 0.1
 * - Function list:
 *   1. 
 *   2. 
 * - Others:
 * - History:
 *   1. Date: 2018.9.4
 *   	Author: zhangzhao
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 ******************************************************************************/
#ifndef __DPDK_CHUNK_STRUCT__
#define __DPDK_CHUNK_STRUCT__

//#define MAX_TYPES 5
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include "dpdk_mempool.h"

#define MAX_CORE 100
#define RTE_MBUF_TYPE
//zhangzhao
#define RTE_MBUF_SIZE sizeof(struct rte_mbuf)
#define TYPE_SIZE_1 199
#define TYPE_SIZE_2 sizeof(struct chunk_head)
#define TYPE_SIZE_3 199
#define TYPE_SIZE_4 199
#define PKT_MBUF_DATA_SIZE 2048
/* data structures only for head*/
/******************************************************************************/
struct chunk_head{
	int id;
	int type;
	int size;
	struct rte_mempool *pool;
	struct rte_mempool *lpool;
	struct chunk_head *next;
	void* re;
};

/******************************************************************************/
/* API functions declarations */
/******************************************************************************/
/*   this part only for test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	*/
/* - Function: int data_type_init_1 ~data_type_init_4
 * - Description: init new mempool struct and get mempool form dpdk hugepages
 * - Parameters:
 *   1. mp: which mempool chunk from
 *   2. obj:target chunk
 * - 3. i:chunk id in mempool
 *   Return:
 *	1:if success
 *	-1:fail
 * - Others:
 * */
int
data_type_init_notype(struct rte_mempool *mp,int type,void *obj, int i);

//typedef void *(*chunk_get_func)(struct Mempool_t *mctx,struct rte_mempool *mp,int num,int core);

//void*
//chunk_alloc_t0(struct Mempool_t *mctx,struct rte_mempool *mp,int num,int core);

void*
chunk_alloc_notype(void *input,struct rte_mempool *mp,int num,int core);
//void* chunk_alloc_t2(struct rte_mempool *mp,int type);
//void* chunk_alloc_t3(struct rte_mempool *mp,int type);
//void* chunk_alloc_t4(struct rte_mempool *mp,int type);


//int
//chunk_free_t0(struct Mempool_t *mctx,void *chunk,int core);
int
chunk_free_notype(void *input,void *chunk,int core);
/******************************************************************************/
/* API functions declarations */
/******************************************************************************/
//int mempool_chunk_init(struct Mempool_t *mem_t);
int
init_chunk_functions(void *input,int type);
#endif
