 /** 
 * @file stream_queue.h 
 * @brief structs and functions for tcp stream queues, supporting multi-in and 
 * 	single-out
 * @author Shenyifan (shenyifan@ict.ac.cn)
 * @date 2018.8.19 
 * @version 1.0
 * @detail Function list: \n
 *   1. steamq_init(): init a stream_queue \n
 *   2. streamq_enqueue(): add a stream to the tail of stream_queue \n
 *   3. streamq_dequeue(): get and remove the stream at the head of 
 *   	stream_queue \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.8.19 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __STREAM_QUEUE_H_
#define __STREAM_QUEUE_H_
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
#include "n21_queue.h"
typedef struct n21_queue tcp_stream_queue;
typedef tcp_stream_queue *stream_queue_t;
typedef struct circular_queue tcp_stream_queue_single;
typedef tcp_stream_queue *stream_queue_singlet;
#include "tcp_stream.h"
/******************************************************************************/
/* global macros */
/******************************************************************************/
/* data structures */
/******************************************************************************/
/* function declarations */
/******************************************************************************/
/* inline functions */
/**
 * init a tcp_stream queue
 *
 * @param q			target stream_queue
 * @param queue_num	num of circular queues
 * @param size 		size of every circular queue 
 * 
 * @return 
 * 	return the queue alloced and inited if success; otherwise return NULL;
 */
static inline void 
streamq_init(stream_queue_t q, uint8_t queue_num, uint32_t size)
{
	n21q_init(q, queue_num, size);
}

/**
 * enqueue a tcp_stream into the target queue
 *
 * @param q 		target tcp_stream queue
 * @param q_id		target queue id for parallel
 * @param stream 	target tcp_stream
 * 
 * @return 
 * 	return SUCCESS if success; otherwise return FAILED
 */
static inline int
streamq_enqueue(stream_queue_t q, uint8_t q_id, tcp_stream_t cur_stream)
{
	return n21q_enqueue(q, q_id, (void*)cur_stream);
}

/**
 * get the first stream in any queue and remove it from the queue
 *
 * @param q target tcp_stream queue
 * 
 * @return
 * 	return the first stream found in the queue; 
 * 	return NULL if all the queues are empty
 */
static inline tcp_stream_t
streamq_dequeue(stream_queue_t q)
{
	return (tcp_stream_t)n21q_dequeue_strong(q);
}
/******************************************************************************/
// stream queue processing for single-producer-single-consumer mode
/**
 * init a tcp_stream queue
 *
 * @param q			target stream_queue
 * @param size 		size of every circular queue 
 * 
 * @return 
 * 	return the queue alloced and inited if success; otherwise return NULL;
 */
static inline void 
sstreamq_init(stream_queue_singlet q, uint32_t size)
{
	cirq_init(q, size);
}

/**
 * enqueue a tcp_stream into the target queue
 *
 * @param q 		target tcp_stream queue
 * @param stream 	target tcp_stream
 * 
 * @return 
 * 	return SUCCESS if success; otherwise return FAILED
 */
static inline int
sstreamq_enqueue(stream_queue_singlet q, tcp_stream_t cur_stream)
{
	return cirq_add(q, (void*)cur_stream);
}

/**
 * get the first stream in any queue and remove it from the queue
 *
 * @param q target tcp_stream queue
 * 
 * @return
 * 	return the first stream found in the queue; 
 * 	return NULL if all the queues are empty
 */
static inline tcp_stream_t
sstreamq_dequeue(stream_queue_singlet q)
{
	return (tcp_stream_t)cirq_get(q);
}
/******************************************************************************/
#endif //#ifdef __STREAM_QUEUE_H_
