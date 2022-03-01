/**
 * @file pri_queue.h 
 * @brief priority queue tool (implemented as heap)
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.8.6
 * @version 1.0 
 * @detail Function list: \n
 *   1. priq_init(): init the pri_queue\n
 *   2. priq_pop(): get the top item and remove it from the pri_queue\n
 *   3. priq_push(): push an item into the pri_queue \n
 *   4. priq_size(): return the num of items in the pri_queue() \n
 *   5. priq_empty(): return whether the pri_queue is empty \n
 *   6. priq_top(): a glance at the top item int he pri_queue \n
 *   7. priq_maintain(): maintain the pri_queue from the given position
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
#ifndef __PRI_QUEUE_H_
#define __PRI_QUEUE_H_
/******************************************************************************/
#include <stdio.h>
#include <stdint.h>
/******************************************************************************/
//#define DBG_PRIQ
#ifdef DBG_PRIQ
	#define TRACE_PRIQ(f, m...) fprintf(stderr, f, ##m)
#else
	#define TRACE_PRIQ(f, m...) (void)0
#endif
/******************************************************************************/
#if 0
typedef uint64_t PRIQ_TYPE;

int
uint64_gt(void *a, void *b)
{ 
	return *((PRIQ_TYPE*)a) < *((PRIQ_TYPE*)b);
}
#endif
// use greater function if the first item is the max one
typedef int(PRIQ_FUNC_CMP)(PRIQ_TYPE *a, PRIQ_TYPE *b);
/******************************************************************************/
struct pri_queue
{
	int max_size;
	int size;
	PRIQ_TYPE *data;
	PRIQ_FUNC_CMP *cmp_func;
};
typedef struct pri_queue priq_t;
/******************************************************************************/
static inline int 
priq_get_left_child(priq_t *q, int p)
{
	int ret = (p << 1) + 1;
	if (ret >= q->size) {
		ret = -1;
	}
	return ret;
}

static inline int
priq_get_right_child(priq_t *q, int p)
{
	int ret = (p << 1) + 2;
	if (ret >= q->size) {
		ret = -1;
	}
	return ret;
}

static inline int 
priq_get_parent(int p)
{
	if (p > 0) {
		return (p - 1) >> 1;
	} else {
		return -1;
	}
}	

/******************************************************************************/
static inline void
priq_init(priq_t *q, int max_size, PRIQ_FUNC_CMP cmp_func)
{
	q->max_size = max_size;
	q->size = 0;
	q->data = (PRIQ_TYPE *)calloc(max_size, sizeof(PRIQ_TYPE));
	q->cmp_func = cmp_func;
}

/**
 * maintain the priority queue
 *
 * @param q		target pri_queue
 * @param p		the start position
 * @param item	the item to put in the final position
 * @param dir	the direction, 0 if up, 1 if down
 *
 * @return 
 * 	return the final position the item should be writen to
 */
static inline int 
priq_maintain(priq_t *q, int p, PRIQ_TYPE item, int dir)
{
	if (dir == 0) {
		// maintain upside
		int parent;
		for (;;) {
			parent = priq_get_parent(p); 
			if (parent != -1 && q->cmp_func(&item, &q->data[parent])) {
				q->data[p] = q->data[parent];
				p = parent;
			} else {
				break;
			}
		}
	} else {
		// maintain downside
		int l_child, r_child, t_child;
		for (;;) {
			l_child = priq_get_left_child(q, p);
			if (l_child == -1) {
				break;
			}
			r_child = priq_get_right_child(q, p);
			if (r_child == -1 || 
					q->cmp_func(&q->data[l_child], &q->data[r_child])) {
				t_child = l_child;
			} else {
				t_child = r_child;
			}
			if (q->cmp_func(&q->data[t_child], &item)) {
				q->data[p] = q->data[t_child];
				p = t_child;
			} else {
				break;
			}	
		}
	}
	q->data[p] = item;

	return p;
}

/**
 * get the top item of the priority queue
 *
 * @param q			targe priority queue
 * @param[out] ret	return the top item to ret
 *
 * @return
 * 	return the position where the last item is finaly add to
 */
static inline int
priq_pop(priq_t *q, PRIQ_TYPE *ret)
{

	int position = 0;
	if (q->size == 0) {
		return -1;
	}
	*ret = q->data[0];
	// maintain from top
	q->size--;
	if (q->size){
		position = priq_maintain(q, 0, q->data[q->size], 1);
	}
	return position;
}

static inline int
priq_push(priq_t *q, PRIQ_TYPE item)
{
	int position;
	if (q->size == q->max_size) {
		return -1;
	}
	// maintain from bottom
	q->size++;
	position = priq_maintain(q, q->size-1, item, 0);
	return position;
}

static inline int 
priq_size(priq_t *q)
{
	return q->size;
}

static inline int
priq_empty(priq_t *q)
{
	return (q->size == 0);
}

static inline int
priq_top(priq_t *q, PRIQ_TYPE *ret)
{
	if (q->size == 0) {
		return -1;
	}
	*ret = q->data[0];
	return 0;
}
/******************************************************************************/
#endif //#ifdef __PRI_QUEUE_H_
