/**
 * @file n21_queue.h
 * @brief lock-free multi-producer-single-consumer queue
 * @author Shen Yifan (shenyifan@ict.ac.cn) 
 * @date 2019.4.2 
 * @version 1.0
 * @detail Function list: \n
 *   1.n21q_init(): initialize n21 queue and allocate memory \n
 *   2.n21q_enqueue(): put an element into the n21_queue \n
 *   3.n21q_dequeue(): get an element from the n21_queue \n
 *   4.n21q_dequeue_strong(): get and element in spite of ready signals \n
 *   5.n21q_get_wslot(): get a writable slot from the n21_queue \n
 *   6.n21q_writen(): sign a writable slot as available \n
 * 	 7.n21q_set_slot(): init slots in the n21_queue
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.4.2 
 *   	Author: Shen Yifan
 *   	Modification: create (seprated from circular.h)
 *   2. Date: 2019.4.2
 *   	Author: Shen Yifan
 *   	Modification: using bitmap when try to dequeue
 *   3. Date: 2019.4.16
 *   	Author: Shen Yifan
 *   	Modification: don't use bitmap, use ready_flag instead
 *   4. Date: 
 *   	Author: Shen Yifan
 *   	Modification:
 */
#ifndef __N21_QUEUE_H_
#define __N21_QUEUE_H_
/******************************************************************************/
struct n21_queue;
typedef struct n21_queue *n21q_t;
/******************************************************************************/
#include "circular_queue.h"
/******************************************************************************/
/* data structures */
//#define N21Q_DEQUEUE_STRONG		// dequeue without looking up flag
//#define N21Q_PREFETCH			// prefetch the next item when dequeue
#define USING_READY_FLAG		0
#define USING_PC_COUNETER		0	// count in/out operations on every core
#if USING_READY_FLAG + USING_PC_COUNETER > 1
	#error :only one fast walk for n21q is permitted!
#endif
#if !(USING_READY_FLAG  || USING_PC_COUNETER)
	#define N21Q_DEQUEUE_STRONG		// dequeue without looking up flag
#endif

struct n21_queue
{
#if USING_READY_FLAG
	volatile uint8_t ready_flag[64];
#endif
#if USING_PC_COUNETER
	volatile uint32_t count_in[MAX_CORE_NUM+1];
	volatile uint32_t count_out[MAX_CORE_NUM+1];
#endif
	uint8_t queue_num;				///< num of circular queues
	uint8_t queue_point;			///< point to the queue going to get item
	struct circular_queue *queues;	///< array of circular queues for parallel
#ifdef N21Q_PREFETCH
	void * prefetched;
#endif
};
/******************************************************************************/
/* function declarations */
/* local inline functions */
static inline uint8_t 
n21q_succ(n21q_t q, uint8_t p)
{
	p++;
	return (p == q->queue_num) ? 0 : p;
}

static inline uint8_t
n21q_fast_check_empty(n21q_t q, uint8_t p)
{
#if USING_READY_FLAG
	return !q->ready_flag[p];
#endif
#if USING_PC_COUNETER
	return q->count_out[p] == q->count_in[p];
#endif
}

static inline uint32_t
n21q_count(n21q_t q)
{
	int i;
	uint32_t ret = 0;
	for (i=0; i<q->queue_num; i++) {
		ret += cirq_count(&q->queues[i]);
	}
	return ret;
}

static inline void *
__n21q_dequeue_strong(n21q_t q)
{
	uint8_t start_point = q->queue_point;
	void *ret = NULL;
	
	do {
		ret = cirq_get(&q->queues[q->queue_point]);
#if USING_PC_COUNETER
		if (ret) {
			q->count_out[q->queue_point]++;
		}
#endif
#if USING_READY_FLAG
		if (!ret) {
			q->ready_flag[q->queue_point] = 0;
		}
#endif
		q->queue_point = n21q_succ(q, q->queue_point);
	} while (!ret && start_point != q->queue_point); 
	
	return ret;
}

static inline void *
n21q_dequeue_local(n21q_t q)
{
	uint8_t start_point = q->queue_point;
	void *ret = NULL;
	
	do {
		ret = cirq_get(&q->queues[q->queue_point]);
#if USING_PC_COUNETER
		if (ret) {
			q->count_out[q->queue_point]++;
		}
#endif
		if (!ret) {
#if USING_READY_FLAG
			q->ready_flag[q->queue_point] = 0;
#endif
			q->queue_point = n21q_succ(q, q->queue_point);
		}
	} while (!ret && start_point != q->queue_point); 
	
	return ret;
}

static inline void *
__n21q_dequeue(n21q_t q)
{
#ifdef N21Q_DEQUEUE_STRONG
	return __n21q_dequeue_strong(q);
#else
	uint8_t start_point = q->queue_point;
	void *ret = NULL;
	
	while (1) {
		// try to find the first queue ready to be get
		if (n21q_fast_check_empty(q, q->queue_point)) {
			// the current queue is set empty, try the next queue
			q->queue_point = n21q_succ(q, q->queue_point);
			if (start_point == q->queue_point) {
				// if all queues are checked, return NULL
				break;
			}
			continue;
		}

		// found a queue which is set ready
		ret = cirq_get(&q->queues[q->queue_point]);
		if (ret) {
	#if USING_READY_FLAG
			if (cirq_empty(&q->queues[q->queue_point])) {
				// if the queue is empty after getting the item, set empty
				q->ready_flag[q->queue_point] = 0;
			}
	#endif
	#if USING_PC_COUNETER
			q->count_out[q->queue_point]++;
	#endif
			// get from the next queue the next time, since it's round robin
			q->queue_point = n21q_succ(q, q->queue_point);
			break;
		} else {
	#if USING_READY_FLAG
			// this queue is empty but set ready, try the next queue
			q->ready_flag[q->queue_point] = 0;
	#endif
			q->queue_point = n21q_succ(q, q->queue_point);
		}
	}
	return ret;
#endif
}
/*----------------------------------------------------------------------------*/
/* global inline functions */
/**
 * init a n21 queue
 *
 * @param q			target n21_queue
 * @param queue_num	num of circular queues
 * @param size 		size of every circular queue 
 * 
 * @return 
 * 	return the queue alloced and inited if success; otherwise return NULL;
 */
static inline void 
n21q_init(n21q_t q, uint8_t queue_num, uint32_t size)
{
	int i;
	q->queue_num = queue_num;
	q->queue_point = 0;
	q->queues = (cirq_t)calloc(queue_num, sizeof(struct circular_queue));
	for (i=0; i<queue_num; i++) {
		cirq_init(&q->queues[i], size);
#if USING_READY_FLAG
		q->ready_flag[i] = 0;
#endif
#if USING_PC_COUNETER
		q->count_in[i] = 0;
		q->count_out[i] = 0;
#endif
	}
}

/**
 * enqueue a item into the target queue
 *
 * @param q 		target n21 queue
 * @param q_id		target queue id for parallel
 * @param item	 	target item
 * 
 * @return 
 * 	return SUCCESS if success; otherwise return FAILED
 */
static inline int
n21q_enqueue(n21q_t q, uint8_t q_id, void* item)
{
	// it's ok though it's not not multi-thread safe
	int ret = cirq_add(&q->queues[q_id], item);
#if USING_READY_FLAG
	_mm_sfence();
	if (0 == q->ready_flag[q_id]) {
		q->ready_flag[q_id] = 1;
 	}
#endif
#if USING_PC_COUNETER
	_mm_sfence();
	q->count_in[q_id] ++;
#endif
	return ret;
} 

/**
 * get the first item in any queue and remove it from the queue
 *
 * @param q 	target n21 queue
 * 
 * @return
 * 	return the first item found in the queue; 
 * 	return NULL if all the queues are empty
 */
static inline void *
n21q_dequeue(n21q_t q)
{
#ifndef N21Q_PREFETCH
	return __n21q_dequeue(q);
#else
	void *ret;
	if (q->prefetched) {
		ret = q->prefetched;
	} else {
		ret = __n21q_dequeue(q);
	}
	q->prefetched = __n21q_dequeue(q);
	return ret;
#endif
}

/**
 * get the first item in any queue and remove it from the queue, without 
 * fast_empty_check
 *
 * @param q 	target n21 queue
 * 
 * @return
 * 	return the first item found in the queue; 
 * 	return NULL if all the queues are empty
 */
static inline void *
n21q_dequeue_strong(n21q_t q)
{
#ifndef N21Q_PREFETCH
	return __n21q_dequeue_strong(q);
#else
	void *ret;
	if (q->prefetched) {
		ret = q->prefetched;
	} else {
		ret = __n21q_dequeue_strong(q);
	}
	q->prefetched = __n21q_dequeue_strong(q);
	return ret;
#endif
}

static inline void **
n21q_get_wslot(n21q_t q, uint8_t q_id)
{
	void **ret = cirq_get_wslot(&q->queues[q_id]);
	return ret;
}

/**
 * init slots in the n21_queue
 *
 * @param q		target n21_queue
 * @param q_id	target queue id
 * @param p		target slot number
 * @param item	target element for initialization
 *
 * @return 
 * 	return SUCCESS if success;
 * 	otherwise return ERROR
 */
static inline void 
n21q_init_slot(n21q_t q, int slot_size)
{
	int i;
	for (i=0; i<q->queue_num; i++) {
		cirq_init_slot(&q->queues[i], slot_size);
	}
}

static inline int 
n21q_writen(n21q_t q, uint8_t q_id)
{
	int ret = cirq_writen(&q->queues[q_id]);
#if USING_READY_FLAG
	_mm_sfence();
	if (0 == q->ready_flag[q_id]) {
		q->ready_flag[q_id] = 1;
 	}
#endif
#if USING_PC_COUNETER
	_mm_sfence();
	q->count_in[q_id] ++;
#endif
	return ret;
}
/******************************************************************************/
#endif //#ifndef __N21_QUEUE_H_
