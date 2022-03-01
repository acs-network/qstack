/**
 * @file topk_priq.h 
 * @brief priority queue with top K elements
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.8.6
 * @version 1.0 
 * @detail Function list: \n
 *   1. tpq_init(): init the topk_pri_queue \n
 *   2. tpq_push(): add an item into the topk_pri_queue \n
 *   3. tpq_pop(): get the top item and remove it from the topk_pri_queue \n
 *   4. tpq_size(): the number of items in the topk_pri_queue \n
 *   5. tpq_empty(): return whether the topk_pri_queue is empty \n
 *   6. tpq_top(): a glance at the top item in the topk_pri_queue 
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.8.6
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __TOPK_PRIQ_H_
#define __TOPK_PRIQ_H_
/******************************************************************************/
#include <stdio.h>
#include <stdint.h>

//#define DBG_TPQ
#ifdef DBG_TPQ
	#define TRACE_TPQ(f, m...) fprintf(stderr, f, ##m)
#else
	#define TRACE_TPQ(f, m...) (void)0
#endif
#if 0
typedef uint64_t TOPK_PRIQ_TYPE;

int
uint64_gt(void *a, void *b)
{ 
	return *((TOPK_PRIQ_TYPE*)a) < *((TOPK_PRIQ_TYPE*)b);
}
#endif
// use greater function if want the top k max items

struct topk_priq_item {
	TOPK_PRIQ_TYPE data;
	int data_p;
	int topk_p;
	int max_p;
};
typedef struct topk_priq_item tpq_item_t;
typedef tpq_item_t *PRIQ_TYPE;
typedef int(TPQ_FUNC_CMP)(TOPK_PRIQ_TYPE *a, TOPK_PRIQ_TYPE *b);
#include "pri_queue.h"

struct topk_priority_queue {
	int max_size;
	int size;
	struct topk_priq_item *data;
	TPQ_FUNC_CMP *cmp_func;

	priq_t topk_heap; // actually it's a min heap
	priq_t max_heap; // to get the largest one
};
typedef struct topk_priority_queue tpq_t;
/******************************************************************************/
void tpq_update_topk(tpq_t *q, int sp, int ep)
{
	priq_t *topk_heap = &q->topk_heap;
	for (;sp >= ep; sp = priq_get_parent(sp)) {
		topk_heap->data[sp]->topk_p = sp;
	}
}
void tpq_update_max(tpq_t *q, int sp, int ep)
{
	priq_t *max_heap = &q->max_heap;
	for (;sp >= ep; sp = priq_get_parent(sp)) {
		max_heap->data[sp]->max_p = sp;
	}
}
/******************************************************************************/
static inline void
tpq_init(tpq_t *q, int max_size, TPQ_FUNC_CMP cmp_func, 
		PRIQ_FUNC_CMP cmp_func_topk, PRIQ_FUNC_CMP cmp_func_max)
{
	q->max_size = max_size;
	q->size = 0;
	q->data = (tpq_item_t *)calloc(max_size, sizeof(tpq_item_t));
	q->cmp_func = cmp_func;

	priq_init(&q->topk_heap, max_size, cmp_func_topk);
	priq_init(&q->max_heap, max_size, cmp_func_max);
}

static inline int
tpq_push(tpq_t *q, TOPK_PRIQ_TYPE item)
{
	int topk_sp, topk_ep, max_sp, max_ep;
	if (q->size < q->max_size) {
		// the queue is not full, directly add item to tail
		q->data[q->size].data = item;
		q->data[q->size].data_p = q->size;
		topk_sp = q->size;
		topk_ep = priq_push(&q->topk_heap, &q->data[q->size]);
		max_sp = q->size;
		max_ep = priq_push(&q->max_heap, &q->data[q->size]);
		TRACE_TPQ("[push] direct push, "
				"data_p: %d topk: %d - %d, max: %d - %d\n", 
				topk_sp, topk_ep, max_sp, max_ep);
		q->size++;
	} else {
		// the queue is full
		if (q->cmp_func(&q->topk_heap.data[0]->data, &item)) {
			return -1;
		}
		// replace the topk's top with item, and it's a leaf of max_heap
		int data_p = q->topk_heap.data[0]->data_p;
		q->data[data_p].data = item;
		topk_sp = 0;
		topk_ep = priq_maintain(&q->topk_heap, 0, q->topk_heap.data[0], 1);
		max_sp = q->data[data_p].max_p;
		max_ep = priq_maintain(&q->max_heap, max_sp, &q->data[data_p], 0);
		TRACE_TPQ("[push] direct push, "
				"data_p: %d topk: %d - %d, max: %d - %d\n", 
				topk_sp, topk_ep, max_sp, max_ep);
	}
	tpq_update_topk(q, topk_ep, topk_sp);
	tpq_update_max(q, max_sp, max_ep);
}

static inline int
tpq_pop(tpq_t *q, TOPK_PRIQ_TYPE *ret)
{
	int topk_sp, topk_ep, max_sp, max_ep, data_p;
	if (q->size == 0) {
		return -1;
	}
	*ret = q->max_heap.data[0]->data;
	q->size--;
	if (q->size == 0) {
		return 0;
	}
	data_p = q->max_heap.data[0]->data_p;
	topk_sp = q->max_heap.data[0]->topk_p;
	max_sp = 0;
	if (data_p != q->size) {
		// move the last item of data to the hole
		q->max_heap.data[0] = &q->data[data_p];
		q->topk_heap.data[topk_sp] = &q->data[data_p];
		q->data[data_p].data = q->data[q->size].data;
	}
	q->max_heap.size--;
	q->topk_heap.size--;
	max_ep = priq_maintain(&q->max_heap, 0, q->max_heap.data[q->size], 1);
	topk_ep = priq_maintain(&q->topk_heap, topk_sp, 
			q->topk_heap.data[q->size], 0);

	TRACE_TPQ("[pop] direct pop, "
			"data_p: %d topk: %d - %d, max: %d - %d\n", 
			topk_sp, topk_ep, max_sp, max_ep);
	tpq_update_max(q, max_ep, max_sp);
	tpq_update_topk(q, topk_sp, topk_ep);
}

static inline int
tpq_size(tpq_t *q)
{
	return q->size;
}

static inline int
tpq_empty(tpq_t *q)
{
	return (q->size == 0);
}

static inline int 
tpq_top(tpq_t *q, TOPK_PRIQ_TYPE *ret)
{
	if (q->size == 0) {
    	return -1;
	}
	*ret = q->max_heap.data[0]->data;
	return 0;
}
/******************************************************************************/
#endif // #ifndef __TOPK_PRIQ_H_
