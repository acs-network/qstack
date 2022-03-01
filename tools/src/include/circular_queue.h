/**
 * @file circular_queue.h
 * @brief lock-free single-producer-single-consumer circular queue
 * @author Shen Yifan (shenyifan@ict.ac.cn) 
 * @date 2018.8.19 
 * @version 1.0
 * @detail Function list: \n
 *   cirq_init(): initialize circular queue and allocate memory \n
 *   cirq_add(): insert an element to the circular queue \n
 *   cirq_get(): get an element from the queue and remove it from queue \n
 *   cirq_full(): check if the queue is already full \n
 *   cirq_empty(): check if the queue is empty \n
 *   cirq_count(): calculate number of items in the ciecular queue \n
 *   cirq_prefetch(): get an element from queue's tail without remove \n
 *	 cirq_get_wslot(): get an writable slot from queue's head without add \n
 * 	 cirq_init_slot(): init slots in the circular queue \n
 *	 cirq_writen(): sign the writable slot as available \n
 *   cirq_destroy(): free the memory chunk in the queue \n
 *   cirq_init_fast(): init the queue with alloc mem chunk from mempool \n
 *   cirq_destroy_fast(): free the mem chunk alloced by cirq_init_fast() \n
 *   dir_cirq_init(): initialize direct circular queue with local memory \n
 *   dir_cirq_add(): insert an element to the direct circular queue \n
 *   dir_cirq_get(): get an element from the direct queue and remove it
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.8 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2018.8.19
 *   	Author: Shen Yifan
 *   	Modification: change functions into inline mode
 *   3. Date: 2019.4.2
 *   	Author: Shen Yifan
 *   	Modification: seprate n21_queue to single file (n21_queue.h)
 *   4. Date: 2019.4.3
 *   	Author: Shen Yifan
 *   	Modification: add rte_ring mode for performance
 *   5. Date: 
 *   	Author: Shen Yifan
 *   	Modification:
 */
#ifndef __CIRCULAR_QUEUE_H_
#define __CIRCULAR_QUEUE_H_
/******************************************************************************/
struct circular_queue;
typedef struct circular_queue * cirq_t;
/******************************************************************************/
// compile controlling macros
/*----------------------------------------------------------------------------*/
/* use rte_ring instead of my own circular queue */
//#define DPDK_RTE_RING		
/*----------------------------------------------------------------------------*/
/* disable shadow head-tail for cacheline optimization */
#define SIMPLE_HEADTAIL		
/******************************************************************************/
//#include <dpdk_mempool.h>
#include <stdint.h>
#include <stdio.h>

#include "ps.h"

#ifdef DPDK_RTE_RING
#include "rte_ring.h"
#endif

#define STATIC_BUFF_SIZE	8
/******************************************************************************/
#ifndef TRACE_EXCP
	#define TRACE_EXCP(f,m...) fprintf(stderr, "[EXCP]" f, ##m);
#endif
#ifndef TRACE_ERR
	#define TRACE_ERR(f,m...) 	do {	\
			fprintf(stderr, "[ERR]@[%10s:%4d] " f,	\
					__FUNCTION__, __LINE__, ##m);	\
			exit(0);	\
		} while(0)
#endif
/******************************************************************************/
/* data structures */
struct circular_queue
{ 
#ifdef DPDK_RTE_RING
	struct rte_ring *ring;
#else
	#ifdef SIMPLE_HEADTAIL
	volatile uint32_t head;	///< index which ready to insert
	union {
		void **queue;
		void *d_queue[STATIC_BUFF_SIZE];
	};
	uint32_t size;	///< queue size
	volatile uint32_t tail;	///< index which ready to get
	#else
	// head and tail for producer
	volatile uint32_t head;	///< index which ready to insert
	uint32_t tail_s; ///< shadow value of tail
	union {
		void **queue;
		void *d_queue[STATIC_BUFF_SIZE];
	};
	uint32_t size;	///< queue size
	// head and tail for consumer
	volatile uint32_t tail;	///< index which ready to get
	uint32_t head_s; ///< shadow value of head
	#endif
#endif
}; 

// this struct is not milti-thread safe! make sure it's only processed by
// one thread (stack thread)
struct fast_cirq_mempool
{
	void **free_queues;
	uint32_t size;			// num of queues in the mempool
	uint32_t point;			// point to the available queue
	uint32_t length;		// length of every queue
};
typedef struct fast_cirq_mempool *fcirq_mp_t;
/******************************************************************************/
/* function declarations */
/* local inline functions */
static inline uint32_t 
cirq_pree(cirq_t q, uint32_t p)
{
#ifdef DPDK_RTE_RING
	TRACE_ERR("should not be called!\n");
#else
	return p? p-1: q->size-1;
#endif
}

static inline uint32_t 
cirq_succ(cirq_t q, uint32_t p)
{
#ifdef DPDK_RTE_RING
	TRACE_ERR("should not be called!\n");
#else
	return (p + 1) % q->size;
#endif
}
/*----------------------------------------------------------------------------*/
/* global inline functions */
/**
 * test if the queue is full, always called by producer
 *
 * @param q		target circular queue
 *
 * @return
 * 	return TRUE if the queue is full; otherwise return FALSE
 * @note
 * 	it may not be multi-thread safe, the result may not be correct
 */
static inline uint32_t 
cirq_full(cirq_t q)
{
#ifdef DPDK_RTE_RING
	return rte_ring_full(q->ring);
#else
	#ifdef SIMPLE_HEADTAIL
	return cirq_succ(q, q->head) == q->tail;
	#else
	uint32_t ret;
	if (cirq_pree(q, q->tail_s) != q->head) {
		ret = FALSE;
	} else {
		if (cirq_pree(q, q->tail) != q->head) {
			// the tail_s is not fresh
			q->tail_s = q->tail;
			ret = FALSE;
		} else {
			ret = TRUE;
		}
	}
	return ret;
	#endif
#endif
}

/**
 * test if the queue is empty, always called by consumer
 * 
 * @param q 	target circular queue
 * 
 * @return 
 * 	return TRUE if the queue is empty; otherwise return FALSE
 * @note
 * 	it may not be multi-thread safe, the result may not be correct
 */
static inline uint32_t
cirq_empty(cirq_t q)
{
#ifdef DPDK_RTE_RING
	return rte_ring_empty(q->ring);
#else
	#ifdef SIMPLE_HEADTAIL
	return q->head == q->tail;
	#else
	uint8_t ret;
	if (q->tail != q->head_s) {
		ret = FALSE;
	} else {
		if (q->tail != q->head) {
			q->head_s = q->head;
			ret = FALSE;
		} else {
			ret = TRUE;
		}
	}
	return ret;
	#endif
#endif
}

/**
 * count how many items in the circular queue
 *
 * @param q 	target ciecular queue
 *
 * @return
 *	return the num of items in the queue
 * @note
 * 	it may not be multi-thread safe, the result may not be correct
 */
static inline uint32_t
cirq_count(cirq_t q)
{
#ifdef DPDK_RTE_RING
	return rte_ring_count(q->ring);
#else
	return q->head>=q->tail ? q->head-q->tail : q->head+q->size-q->tail;
#endif
}

/**
 * init circular queue and allocate memory for queue
 * 
 * @param q 	target circular queue 
 * @param size 	max size of queue
 * 
 * @return void
 */
static inline void 
cirq_init(cirq_t q, uint32_t size)
{
#ifdef DPDK_RTE_RING
	char name[64];
	sprintf(name, "%p", &q->ring);
	size = 32;
	q->ring = rte_ring_create(name, size, rte_socket_id(), 
			RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (NULL == q->ring) {
		TRACE_ERR("failed to alloc rte_ring named %s for circular_queue\n", name);
	} else {
	//	TRACE_EXIT("rte_ring named %s is alloced\n", name);
	}
#else
	q->head = 0;
	q->tail = 0;
	#ifndef SIMPLE_HEADTAIL
	q->head_s = 0;
	q->tail_s = 0;
	#endif
	q->size = size;
	q->queue = calloc(size, sizeof(void*));
#endif
}

/**
 * add an element into circular queue's head
 *
 * @param q 	target circular queue
 * @param item 	element to be inserted
 * 
 * @return 
 * 	return FAILED if the queue is full; otherwise return SUCCESS means succeed
 * @see ERROR @see SUCCESS
 */
static inline int 
cirq_add(cirq_t q, void *item)
{
	int ret;
#ifdef DPDK_RTE_RING
	ret = rte_ring_enqueue(q->ring, item);
	if (!ret) {
		return SUCCESS;
	} else {
		return FAILED;
	}
#else
	if (cirq_full(q)) {
	#ifndef FULL_CIRQ_ISOK
		TRACE_EXCP("full circular queue\n");
	#endif
		return FAILED;
	}
	
	q->queue[q->head] = item;
	_mm_sfence();
	q->head = cirq_succ(q, q->head);
	return SUCCESS;
#endif
} 

/**
 * get the element from the circular queue's tail
 * 
 * @param q 	target circular queue
 * 
 * @return 
 * 	return NULL if the queue is empty; otherwise return the pointer to the
 * 	element of queue's tail and remove it from the queue
 */
static inline void *
cirq_get(cirq_t q)
{
#ifdef DPDK_RTE_RING
	void *ret = NULL;
	rte_ring_dequeue(q->ring, &ret);
	return ret;
#else
	void *ret;
	if (cirq_empty(q)) {
		return NULL;
	}
	ret = q->queue[q->tail];
	_mm_sfence();
	q->tail = cirq_succ(q, q->tail);
	return ret;
#endif
}

/**
 * get the element from queue's tail without remove it 
 * 
 * @param q 	target circular queue
 * 
 * @return 
 * 	return NULL if the queue is empty; otherwise return the pointer to the
 * 	element of queue's tail
 */
static inline void *
cirq_prefetch(cirq_t q)
{
#ifdef DPDK_RTE_RING
	TRACE_TODO("not available for dpdk mode\n");
#else
	return cirq_empty(q)? NULL : q->queue[q->tail];
#endif
}

/**
 * get a writable slot from the head of cirq
 *
 * @param q		target circular queue
 *
 * @return
 * 	return pointer to the writable slot
 */
static inline void **
cirq_get_wslot(cirq_t q)
{
#ifdef DPDK_RTE_RING
	TRACE_TODO("not available for dpdk mode\n");
#else
	return &q->queue[q->head];
#endif
}

/**
 * sign the writable slot as available
 *
 * @param q		target circular queue
 *
 * @return
 * 	return SUCCESS if success; 
 * 	otherwise return FAILED (the queue is full)
 */
static inline int
cirq_writen(cirq_t q)
{
#ifdef DPDK_RTE_RING
	TRACE_TODO("not available for dpdk mode\n");
#else
	if (cirq_full(q)) {
		return FAILED;
	}
	q->head = cirq_succ(q, q->head);
	return SUCCESS;
#endif
}

/**
 * init slots in the circular queue
 *
 * @param q				target circular queue
 * @param slot_size		size of the buffer pointed by every slot
 *
 * @return null
 */  
static inline void 
cirq_init_slot(cirq_t q, int slot_size)
{
	int i;
	uint8_t *mem;

#ifndef DPDK_RTE_RING
	if (!q->queue) {
		TRACE_EXCP("try to init the slots in a circular queue "
				"without initialization!");
		exit(0);
	}
	mem = (uint8_t *)calloc(q->size, slot_size);
	for (i=0; i<q->size; i++) {
		q->queue[i] = mem + i * slot_size;
	}
#endif
}

/**
 * free the members alloced in the circular queue
 *
 * @param q 	target circular queue
 *
 * @return null
 */
static inline void 
cirq_destroy(cirq_t q)
{
	if (!cirq_empty(q)) {
		TRACE_EXCP("try to destroy a circular_queue which is not empty!\n");
	}
#ifdef DPDK_RTE_RING
	rte_ring_free(q->ring);
#else
	free(q->queue);
#endif
} 

/**
 * trace circular queue's detail info
 *
 * @param q		target circular queue
 * 
 * @return null
 * @note
 * 	it may not be multi-thread safe, the result may not be correct
 */
static inline void 
cirq_print_info(cirq_t q)
{
#ifdef DPDK_RTE_RING
#else
	TRACE_DETAIL("queue head:%u, queue tail:%u, queue len:%u\n", 
			q->head, q->tail, cirq_count(q));
#endif
}

/**
 * scan the circular queue to find a same item
 *
 * @param q		target circular queue
 * @param item	target item to find
 *
 * @return
 * 	return the location of item if find it in the queue;
 * 	otherwise return -1
 */
static inline int
cirq_scan(cirq_t q, void *item)
{
	int i = q->tail;
	if (cirq_empty) {
		return -1;
	}
	if (q->head > q->tail) {
		for (; i<q->head; i++) {
			if (q->queue[i] == item) {
				return i;
			}
		}
	} else {
		for (; i<q->size; i++) {
			if (q->queue[i] == item) {
				return i;
			}
		}
		for (i=0; i<q->head; i++) {
			if (q->queue[i] == item) {
				return i;
			}
		}
	}
	return -1;
}
/******************************************************************************/
/******************************************************************************/
/* Fast circular_queues alloc memory from mempool, benefit to queues which    */
/* would be allocated and freed frequently.									  */
/* It is specially designed for rcvbuf and sndbuf of every stream.			  */
/******************************************************************************/
/**
 * init the mempool for fast_cirq
 *
 * @param mp		target mempol
 * @param size		num of queues in the mempool
 * @param length	length of every queue in the mempool
 *
 * @return null
 */
static inline void
fast_cirq_mempool_init(fcirq_mp_t mp, int size, int length)
{
	int i;
	mp->size = size;
	mp->point = size - 1;
	mp->length = length;
	mp->free_queues = calloc(size, sizeof(struct circular_queue));
#ifdef DPDK_RTE_RING
	char name[64];
	for (i=0; i<size; i++) {
		sprintf(name, "%p", &mp->free_queues[i]);
		mp->free_queues[i] = (void*)rte_ring_create(
				name, length, rte_socket_id(), 
				RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
		if (NULL == mp->free_queues[i]) {
			TRACE_ERR("failed to alloc rte_ring named %s "
					"for fast_circular_queue total nums %d len is %d \n", name,size ,length );
		} else {
			printf("rte_ring named %s is alloced for fast_circular_queue\n",
					name);
		}
	}
#else
	void *tmp = calloc(size, length*sizeof(void *));
	for (i=0; i<size; i++) {
		mp->free_queues[i] = tmp + i*length*sizeof(void *);
	}
#endif
}

static inline void*
fast_cirq_mempool_alloc(fcirq_mp_t mp)
{
	if (mp->point) {
		return mp->free_queues[mp->point--];
	} else {
		TRACE_ERR("empty fast_cirq_mempool!\n");
	}
}

static inline void
fast_cirq_mempool_free(fcirq_mp_t mp, void* item)
{
	if (mp->point == mp->size-1) {
		TRACE_ERR("try to free chunk back to a full mempool!\n");
	} else {
		mp->free_queues[mp->point++] = item;
	}
}

/**
 * init the queue and alloc the chunk from pre-alloced mempool
 *
 * @param q			target circular queue
 * @param mp		where the rte_ring alloced from
 *
 * @return 
 *  return SUCCESS if sucess; otherwise reurn FALSE
 *
 * @note
 *  only dpdk rte_ring mode is available now, default mode is still alloced
 *  from kernel using calloc()
 * @note
 *  fast_cirq_mempool is not multi-thread safe!
 */
static inline int
cirq_init_fast(cirq_t q, fcirq_mp_t mp)
{
#ifdef DPDK_RTE_RING
	q->ring = (struct rte_ring*)fast_cirq_mempool_alloc(mp);
#else
	q->head = 0;
	q->tail = 0;
	q->size = mp->length;
	q->queue = (void **)fast_cirq_mempool_alloc(mp);
#endif
	return SUCCESS;
} 

/**
 * free the mem chunk alloced by cirq_init_fast()
 *
 * @param q				target circular queue
 * @param mp		where the rte_ring alloced from
 *
 * @return null
 *
 * @note
 *  now is only used for mbuf_queue
 * @note
 *  fast_cirq_mempool is not multi-thread safe!
 */
static inline void
cirq_destroy_fast(cirq_t q, fcirq_mp_t mp)
{
	if (!cirq_empty(q)) {
		TRACE_ERR("try to fast_destroy a circular_queue which is not empty!\n");
	}
#ifdef DPDK_RTE_RING
	fast_cirq_mempool_free(mp, q->ring);
#else
	fast_cirq_mempool_free(mp, q->queue);
#endif
}
/******************************************************************************/
/**
 * initialize a direct circular where the queue is in the local memory
 *
 * @param q		target circular queue
 *
 * @return
 * 	will always return SUCCESS for success
 */
static inline int
dir_cirq_init(cirq_t q)
{
#ifdef DPDK_RTE_RING
	char name[64];
	int size = 2048;
	sprintf(name, "%p", &q->ring);
	q->ring = rte_ring_create(name, size, rte_socket_id(), 
			RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
	if (NULL == q->ring) {
		TRACE_ERR("failed to alloc rte_ring named %s for circular_queue\n", name);
	} else {
		TRACE_DETAIL("rte_ring named %s is alloced\n", name);
	}
#else
	q->head = 0;
	q->tail = 0;
	q->size = STATIC_BUFF_SIZE;
#endif
	return SUCCESS;
}

/**
 * insert an element to the direct circular queue
 *
 * @param q		target circular queue
 * @param item	the item to be added
 *
 * @return
 * 	return SUCCESS if success;
 * 	otherwize return FAILED as the queue is already full
 */
static inline int
dir_cirq_add(cirq_t q, void* item)
{
	int ret;
#ifdef DPDK_RTE_RING
	ret = rte_ring_enqueue(q->ring, item);
	if (!ret) {
		return SUCCESS;
	} else {
		return FAILED;
	}
#else
	if (cirq_full(q)) {
	#ifndef FULL_CIRQ_ISOK
		TRACE_EXCP("full circular queue\n");
	#endif
		return FAILED;
	}
	
	q->d_queue[q->head] = item;
	_mm_sfence();
	q->head = cirq_succ(q, q->head);
	return SUCCESS;
#endif
}

/**
 * get an element from the direct circular queue and remove it from the queue 
 *
 * @param q		target circular queue
 *
 * @return
 * 	return the element if success;
 * 	otherwise return NULL
 */
static inline void*
dir_cirq_get(cirq_t q)
{
#ifdef DPDK_RTE_RING
	void *ret = NULL;
	rte_ring_dequeue(q->ring, &ret);
	return ret;
#else
	void *ret;
	if (cirq_empty(q)) {
		return NULL;
	}
	ret = q->d_queue[q->tail];
	_mm_sfence();
	q->tail = cirq_succ(q, q->tail);
	return ret;
#endif
}
/******************************************************************************/
#endif //#ifndef __CIRCULAR_QUEUE_H_
