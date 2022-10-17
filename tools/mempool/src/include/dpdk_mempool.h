
/*******************************************************************************
 * - File name: dpdk_mempool.h 
 * - Author: zhangzhao (mail:zhangzhao@ict.ac.cn) 
 * - Date: 2018.8.29
 * - Description: functions for sending out ip packets
 * - Version: 0.1
 * - Function list:
 *   1.mempool_create( int csize, uint32_t p_pool_size ,uint32_t s_pool_size, int core_num);
 *   2.mempool_alloc_chunk( struct Mempool_t* mp,int core_id);
 *   3.mempool_free_chunk(struct Mempool_t *mp, void *chunk,int core_id);
 *	 4.mempool_destroy(struct Mempool_t* mp);
 * - Others:
 * - History:
 *   1. Date: 2018.7.13
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 ******************************************************************************/
#ifndef __DPDK_MEMPOOL_STRUCT__
#define __DPDK_MEMPOOL_STRUCT__
//#define DEBUG 1
#define MAX_TYPES 20
#define MAX_CPUS 40
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>


struct Mempool_t; 
typedef struct Mempool_t *qmempool_t;
void * mempool_alloc_chunk(struct Mempool_t* mp, int core_id);
int mempool_free_chunk(struct Mempool_t *mp, void *chunk,int core_id);
struct Mempool_t *mempool_create(int csize, uint32_t p_pool_size, 
		uint32_t s_pool_size, int core_num);
#include "universal.h"
#include "dpdk_chunk.h"

typedef const int  (*chunk_init_func)(struct rte_mempool *mp,void *target,int i);
typedef const void *(*chunk_get_func)(void *mctx,struct rte_mempool *mp,int num,int core);
typedef const int (*chunk_free_func)(void *mctx,void *chunk,int core);
/* data structures only for test*/
/******************************************************************************/
/*this data structures only for mempool test
 *
 *qing stack do not use them
*/

/* data structures */
/******************************************************************************/

struct Mempool_t 
{
	int type;  				/*mempool_t memory space type.there are six types of mempools*/
	int max_core; 				/* max core nums */
	int size; 				/* chunk size in mempool */

#if 1
	chunk_init_func chunk_init;
	chunk_get_func chunk_get;
	chunk_free_func chunk_free;
#endif
	const struct rte_memzone * mz[MAX_CPUS + 1]; 	/*get memzone from dpdk hugepages,each cores has it's own memzone and it's support NUMA*/

	struct rte_mempool * mempool_p[MAX_CPUS]; 	/*each core's private mempool*/

	struct rte_mempool * mempool_s;  	/* public mempool,when private mempool is empty we get memory space from public pool */	

//	int type;  				/*mempool_t memory space type.there are six types of mempools*/
//	const struct func_list *chunk_func;

	int l_len[MAX_CPUS];
	void *l_pkt[MAX_CPUS];
	void *l_hpkt[MAX_CPUS];

	int s_len[MAX_CPUS];
	void *s_pkt[MAX_CPUS];
	void *s_hpkt[MAX_CPUS];
//	void *s_array[MAX_CPUS][32];
	int cur_id;
}__rte_cache_aligned;
typedef struct Mempool_t *qmempool_t;

extern struct Mempool_t mempool_t[MAX_TYPES];



/******************************************************************************/
/* API functions declarations */
/******************************************************************************/
/* - Function: struct mempool_t * mempool_create
 * - Description: init new mempool struct and get mempool form dpdk hugepages
 * - Parameters:
 *   @param: int csize: chunk size in mempool
 *   @param: uint32_t p_pool_size:public pool size
 * - @param: uint32_t s_pool_size:private pool size
 *   @param: int core_num:max core_num
 *   @Return:
 *	NULL:if failed
 *	NOT NULL:a point to mempool's space 
 * - Others:
 * */
struct Mempool_t *
mempool_create(int csize, uint32_t p_pool_size, uint32_t s_pool_size, 
		int core_num);

/* - Function: void * mempool_alloc_chunk
 * - Description: get chunk space from mempoot_t
 * - Parameters:
 *   @param: struct mempool_* mp:  mempool for get chunk
 *   @param: int core_id:alloc cpu core id
 *   @Return:
 *	NULL:if failed
 *	NOT NULL:a point to mempool's space 
 * - Others:
 * */
void * 
mempool_alloc_chunk(struct Mempool_t* mp, int core_id);

/* - Function: void * mempool_free_chunk
 * - Description: free chunk space back to  mempoot_t
 * - Parameters:
 *   @param: struct mempool_* mp:  mempool for receive target chunk
 *   @param: void * chunk:free target 
 *   @Return:
 *	1:if free success
 *	-1: fail
 * - Others:
 * */
int 
mempool_free_chunk(struct Mempool_t *mp, void *chunk,int core_id);

/* - Function: void * mempool_destroy
 * - Description: free  mempoot_t back to hugepages
 * - Parameters:
 *   @param: struct mempool_* mp:  free target
 *   @Return:
 *	1:if free success
 *	-1: fail
 * - Others:
 * */
int 
mempool_destroy(struct Mempool_t* mp);
/******************************************************************************/
/* private functions declarations */
/******************************************************************************/
/* - Function: const struct rte_memzone * rte_memzone_get
 * - Description: get mempool  from  memzone
 *
 * - Parameters:
 *   @param name
 *   The name of the memzone. If it already exists, the function will
 *   fail and return NULL.
 *   @param len
 *   The size of the memory to be reserved. If it
 *   is 0, the biggest contiguous zone will be reserved.
 *   @param socket_id
 *   The socket identifier in the case of
 *   NUMA. The value can be SOCKET_ID_ANY if there is no NUMA
 *   constraint for the reserved zone.
 *   @param flags
 *   The flags parameter is used to request memzones to be
 *   taken from specifically sized hugepages.
 *   - RTE_MEMZONE_2MB - Reserved from 2MB pages
 *   - RTE_MEMZONE_1GB - Reserved from 1GB pages
 *   - RTE_MEMZONE_16MB - Reserved from 16MB pages
 *   - RTE_MEMZONE_16GB - Reserved from 16GB pages
 *   - RTE_MEMZONE_256KB - Reserved from 256KB pages
 *   - RTE_MEMZONE_256MB - Reserved from 256MB pages
 *   - RTE_MEMZONE_512MB - Reserved from 512MB pages
 *   - RTE_MEMZONE_4GB - Reserved from 4GB pages
 *   - RTE_MEMZONE_SIZE_HINT_ONLY - Allow alternative page size to be used if
 *   @Return:
 *	NOT NULL: if success
 *	NULL: fail
 * - Others:
*/
const struct rte_memzone * 
rte_memzone_get(const char *name, size_t len, int socket_id, unsigned flags);

/* - Function: struct rte_mempool * rte_private_pool_create
 * - Description: get private mempool  from  memzone and init each chunk
 * - Parameters:
	@param name: target mempool's name
	@param n: init chunk nums
	@param cache_size:for cache line,目前为固定值,用于cache对齐,不同type的chunk 的size不同
	@param priv_size:chunk's
	@core_id: cpu id,each cpu has it's own mempool
	@type: chunk type
 *   @Return:
 *	NOT NULL: if success
 *	NULL: fail
 * - Others:
*/
#if 0
static struct rte_mempool *
rte_private_pool_create(const char *name, unsigned n, unsigned cache_size, 
		uint16_t priv_size, int core_id, int type);

/* - Function: struct rte_mempool * rte_chunk_pool_create
 * - Description: get mempool  from  memzone and init each chunk
 *
 * - Parameters:
	@param name: target mempool's name
	@param n: init chunk nums
	@param cache_size:for cache line,目前为固定值,用于cache对齐,不同type的chunk 的size不同
	@param priv_size:chunk's
	@core_id: cpu id,each cpu has it's own mempool
	@type: chunk type
 *  @Return:
 *	NOT NULL: if success
 *	NULL: fail
 * - Others:
*/

static struct rte_mempool *
rte_chunk_pool_create(const char *name, unsigned n, unsigned cache_size, 
		uint16_t priv_size, int core_id, struct Mempool_t * targ);
#endif
#if 0
/* - Function: void * rte_pool_create
 * - Description: get mempool,
 *   if type is 0 ,this is rte_mbuf pool,use rte_mbuf_pool create to get target mempool
 *   else use rte_private_pool_create to get mempool from memzone
 *
 * - Parameters:
	@param name: target mempool's name
	@param n: init chunk nums
	@param cache_size:for cache line,目前为固定值,用于cache对齐,不同type的chunk 的size不同
	@param priv_size:chunk's
	@core_id: cpu id,each cpu has it's own mempool
 *   @Return:
 *	1: if success
 *	-1: fail
 * - Others:
*/
static struct rte_mempool *
rte_pool_create(const char *name, unsigned n, unsigned cache_size, 
		uint16_t priv_size, int core_id, struct Mempool_t * targ);
#endif
#if 0
/* - Function: void * rte_mempool_freeback
 * - Description: free mempool back to memzone
 *
 * - Parameters:
	@param mp: target mempool 
	@param targ, target Mempool_t
 *   @Return:
 *	1: if success
 *	-1: fail
 * - Others:
*/
	
static  int rte_mempool_freeback(const struct rte_mempool *mp,int type);

/* - Function: void * rte_chunk_alloc
 * - Description: get  chunk from target mempool
 *
 * - Parameters:
	@param mp: target mempool 
	@param type, chunk type,chunk本身会根据type强制转换类型
 *   @Return:
 *	NOT NULL: if success
 *	NULL: fail
 * - Others:
*/
static int rte_mempool_free_chunk(const struct rte_mempool *mp);


/* - Function: void * rte_chunk_alloc
 * - Description: get  chunk from target mempool
 *
 * - Parameters:
	@param mp: target mempool 
	@param type, chunk type,chunk本身会根据type强制转换类型
 *   @Return:
 *	NOT NULL: if success
 *	NULL: fail
 * - Others:
*/

static void* rte_chunk_alloc(struct rte_mempool *mp,int type);

/* - Function: void * rte_chunk_free
 * - Description: free  chunk to mempool
 *
 * - Parameters:
	@param chunk,待释放的空间
	@param type, 待释放空间的type,chunk本身会根据type强制转换类型,然后根据对应的类型结构体中的参数,找到释放回的pool位置
 *   @Return:
 *	1:if free success
 *	-1: fail
 * - Others:
*/

static int  rte_chunk_free(void *chunk,int type);
#endif
#endif
