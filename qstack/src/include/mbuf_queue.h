 /**
 * @file mbuf_queue.h 
 * @brief structs and functions for mbuf queues
 * @author Shenyifan (shenyifan@ict.ac.cn)
 * @date 2018.11.15 
 * @version 1.0
 * @detail Function list: \n
 *   1. mbufq_init(): init a mbuf_queue \n
 *   2. mbufq_enqueue(): add a mbuf to the tail of mbuf_queue \n
 *   3. mbufq_dequeue(): get and remove the mbuf at the head of 
 *   	mbuf_queue \n
 *   4. mbufq_empty(): check if the mbuf_queue is empty \n
 *   5. mbufq_clear(): free the contents in the mbuf queue
 *   6. mbufq_mp_init(): init the mempool for fast mbuf_queue alloc
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.11.15 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2019.3.12
 *   	Author: Shen Yifan
 *   	Modification: add mbufq_clear
 *   3. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __MBUF_QUEUE_H_
#define __MBUF_QUEUE_H_
/******************************************************************************/
/* forward declarations */
#include "circular_queue.h"
typedef struct circular_queue mbuf_queue;
typedef mbuf_queue *mbuf_queue_t;
typedef struct fast_cirq_mempool mbufq_mempool;
typedef mbufq_mempool *mbufq_mp_t;
/******************************************************************************/
#include "mbuf.h"
/******************************************************************************/
/* global macros */
#ifndef DPDK_RTE_RING
	#define DIRECT_CIRCULAR_QUEUE
#endif
/******************************************************************************/
/* data structures */
/******************************************************************************/
/* function declarations */
/******************************************************************************/
/* inline functions */
/**
 * init the mempool for fast mbuf_queue alloc
 *
 * @param mp		target mempol
 * @param size		num of queues in the mempool
 * @param length	length of every queue in the mempool
 *
 * @return null
 */
static inline void
mbufq_mp_init(mbufq_mp_t mp, int size, int length)
{
#ifndef DIRECT_CIRCULAR_QUEUE
	fast_cirq_mempool_init(mp, size, length);
#endif
}

/**
 * init a mbuf queue
 * alloc from mempool when using dpdk mode
 *
 * @param q			target mbuf queue
 * @param mp		where the mem chunk alloced from
 * 
 * @return 
 * 	return SUCCESS if success; otherwise return FALSE;
 */
static inline int
mbufq_init(mbuf_queue_t q, mbufq_mp_t mp)
{
#ifndef DIRECT_CIRCULAR_QUEUE
	return cirq_init_fast(q, mp);
#else
	return dir_cirq_init(q);
#endif
}

/**
 * enqueue a mbuf into the target queue
 *
 * @param q 		target mbuf queue
 * @param mbuf 		target mbuf
 *
 * @return 
 * 	return SUCCESS if success; otherwise return FAILED
 */
static inline int
mbufq_enqueue(mbuf_queue_t q, mbuf_t mbuf)
{
#ifndef DIRECT_CIRCULAR_QUEUE
	return cirq_add(q, (void*)mbuf);
#else
	return dir_cirq_add(q, (void*)mbuf);
#endif
}

/**
 * get the first mbuf in the queue and remove it from the queue
 *
 * @param q 	target mbuf queue
 * 
 * @return
 * 	return the first mbuf in the queue; return NULL if the failed
 */
static inline mbuf_t
mbufq_dequeue(mbuf_queue_t q)
{
#ifndef DIRECT_CIRCULAR_QUEUE
	return (mbuf_t)cirq_get(q);
#else
	return (mbuf_t)dir_cirq_get(q);
#endif
}

/**
 * test if the queue is empty
 *
 * @param q 	target mbuf queue
 * 
 * @return
 * 	return TURE if the queue is empty; otherwise return FAILED
 */
static inline int
mbufq_empty(mbuf_queue_t q)
{
	return cirq_empty(q);
}

/**
 * free the contents in the mbuf queue
 *
 * @param core_id		the core where the function is called
 * @param q				target mbuf queue
 *
 * @return null
 */
static inline void
mbufq_clear(int core_id, mbuf_queue_t q, mbufq_mp_t mp)
{
	mbuf_t mbuf = NULL;
	while (mbuf = mbufq_dequeue(q)) {
		mbuf_set_op(mbuf, MBUF_OP_CLEAR_FREE, core_id);
		mbuf_free(core_id, mbuf);
	}
#ifndef DIRECT_CIRCULAR_QUEUE
	cirq_destroy_fast(q, mp);
#endif
}

/**
 * trace the queue information and mbufs in the queue
 *
 * @param q		target mbuf queue
 *
 * @return null
 */
static inline void
mbufq_print_info(mbuf_queue_t q)
{
#if TRACE_LEVEL >= TRACELV_DETAIL
	#ifndef DPDK_RTE_RING
	int iter = q->tail;
	cirq_print_info(q);
	while (iter != q->head) {
		mbuf_print_info(q->queue[iter]);
		iter = cirq_succ(q, iter);
	}
	#endif
#endif
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#endif //#ifdef __MBUF_QUEUE_H_
