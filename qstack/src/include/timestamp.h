/**
 * @file timestamp.h 
 * @brief timestamps to estimate processing delays
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.11.25
 * @version 1.0
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.11.25
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2019.1.13
 *   	Author: Shen Yifan
 *   	Modification: remove old timestamp, add stage_timeout_test
 */
/******************************************************************************/
#ifndef __TIMESTAMP_H_
#define __TIMESTAMP_H_
/******************************************************************************/
#include "universal.h"
#include "mbuf.h"
/******************************************************************************/
/* global macros */
#if REQ_STAGE_TIMESTAMP && !MBUF_QTS
	#error q_ts in mbuf_t is not available!
#endif
/******************************************************************************/
/* forward declarations */
struct tcp_stream;
/******************************************************************************/
/* data structures */
// timestamp for request-response delay estimating
#define REQ_ST_PKTIN 		0 // packet was received from NIC by io_recv_check()
#define REQ_ST_REQIN 		1 // request was put in the recieve buff
#define REQ_ST_REQREAD 		2 // request was read in app by q_read
#define REQ_ST_RSPWRITE 	3 // response was sent from app by q_write()
#define REQ_ST_RSPGET		4 // response was get from sndbuf
//#define REQ_ST_RSPTCPGEN	5 // start to generate response's tcp header
#define REQ_ST_RSPOUT 		6 // response was sent to driver by io_send_mbuf()
#define REQ_ST_RSPSENT		7
//#define REQ_ST_PKTOUT 		4 // response was sent to NIC by io_send_check()
#define REQ_ST_DISTRI           8
#define REQ_ST_REDIS            9
#define REQ_ST_WORKER           10
struct req_stage_timestamp
{
#if REQ_STAGE_TIMESTAMP
	#ifdef RSTS_SAMPLE
	uint32_t count:31,
			 may_lost:1;
	#endif
	uint64_t ts[20];
#endif
};
typedef struct req_stage_timestamp *rs_ts_t;
/******************************************************************************/
/* function declarations */
// local functions
/******************************************************************************/
// global functions
/**
 * detect timeout between stages
 *
 * @param prev_ts		timestamp at last estimate point (in us)
 * @param thresh		timeout threshhold (in us)
 *
 * @return
 * 	return 0 if not timeout; otherwise return in interval between check
 */
static inline uint64_t
stage_timeout_test(uint64_t *prev_ts, uint64_t thresh)
{
#if STAGE_TIMEOUT_TEST_MODE
	uint64_t cur_ts = get_time_us();
	uint64_t interval = cur_ts - *prev_ts;
	*prev_ts = cur_ts;
	if (interval < thresh) {
		interval = 0;
	}
	return interval;
#endif
}
/******************************************************************************/
// request-response delay estimate
static inline void
rs_ts_add(rs_ts_t ts, uint8_t stage)
{
#if REQ_STAGE_TIMESTAMP
	if (!ts) {
		return;
	}
	ts->ts[stage] = get_time_ns();
#endif
}

static inline void
rs_ts_clear(mbuf_t mbuf)
{
#if REQ_STAGE_TIMESTAMP
	rs_ts_t ts = mbuf->q_ts;
	if (ts) {
		ts->ts[0] = 0;
		mbuf->q_ts = NULL;
	}
#endif
}

/**
 * init the req-rsp timestamp
 *
 * @param ts 	target timestamp
 *
 * @return 
 *  if the timestamp is available, init it and return SUCCESS; 
 *  otherwise return FALSE
 *
 * @note
 * 	This is always called when the packet is received by driver.
 * 	The timestamp is a unit object with memory allocated in qstack, mark the 
 * timestamp if the only handel for ts is not used in stack thread.
 */ 
static inline void
rs_ts_start(rs_ts_t ts, mbuf_t mbuf)
{
#if REQ_STAGE_TIMESTAMP
	mbuf->q_ts = NULL;

	if (mbuf->data_len < 70) {
		return;
	}
	#ifdef RSTS_SAMPLE
	if (likely(ts->count)) {
		if (unlikely(ts->count == RSTS_SAMPLE_CYCLE - 1)) {
			ts->count = 0;
		} else {
			ts->count++;
		}
		return;
	} else {
		ts->count++;
	}
	#endif
	if (!ts->ts[0] || ts->may_lost) {
		ts->ts[0] = get_time_ns();
		mbuf->q_ts = ts;
		ts->may_lost = 0;
	} else {
		ts->may_lost = 1;
	}
#endif
}

/*
 * this is always called when the response packet is acked_free
 */
static inline void
rs_ts_check(mbuf_t mbuf)
{
#if REQ_STAGE_TIMESTAMP
	rs_ts_t ts = mbuf->q_ts;
	if (likely(!ts)) {
		return;
	}
	if (!mbuf->payload_len) {
		return;
	}
	
	uint64_t cur_ts = get_time_ns();
	if (cur_ts - ts->ts[0] > REQ_STAGE_TIMEOUT_THREASH) {
		TRACE_TRACE("request-response timeout!\n"
				"full stage:\t\t%16llu\n"
				"wait in Rx_pool:\t%16llu\n"
				"wait in rcvbuf:\t\t%16llu\n"	
				"processed by app:\t%16llu\n"
				"----distri req:\t\t%16llu\n"
				"----redis pro:\t\t%16llu\n"
				"----q_write call:\t%16llu\n"
				"wait in sndbuf:\t\t%16llu\n"	
				"packet write:\t\t%16llu\n"
				"io_send_mbuf:\t\t%16llu\n"
				"wait in Tx_pool:\t%16llu\n"
				, cur_ts - ts->ts[REQ_ST_PKTIN]
				, ts->ts[REQ_ST_REQIN] - ts->ts[REQ_ST_PKTIN]
				, ts->ts[REQ_ST_REQREAD] - ts->ts[REQ_ST_REQIN]
				, ts->ts[REQ_ST_RSPWRITE] - ts->ts[REQ_ST_REQREAD]
				, ts->ts[REQ_ST_DISTRI] - ts->ts[REQ_ST_REQREAD]
				, ts->ts[REQ_ST_WORKER] - ts->ts[REQ_ST_REDIS]
				, ts->ts[REQ_ST_RSPWRITE] - ts->ts[REQ_ST_WORKER]
				, ts->ts[REQ_ST_RSPGET] - ts->ts[REQ_ST_RSPWRITE]
				, ts->ts[REQ_ST_RSPOUT] - ts->ts[REQ_ST_RSPGET]
				, ts->ts[REQ_ST_RSPSENT] - ts->ts[REQ_ST_RSPOUT]
				, cur_ts - ts->ts[REQ_ST_RSPSENT]
				);
	}

	// clear the ts for next cycle to use
	rs_ts_clear(mbuf);
#endif
}

static inline void
rs_ts_pass(mbuf_t mbuf_1, mbuf_t mbuf_2)
{
#if REQ_STAGE_TIMESTAMP
	mbuf_2->q_ts = mbuf_1->q_ts;
	mbuf_1->q_ts = NULL;
#endif
}

#if REQ_STAGE_TIMESTAMP
void
rs_ts_pass_to_stream(mbuf_t mbuf, struct tcp_stream *cur_stream);

void
rs_ts_pass_from_stream(struct tcp_stream *cur_stream, mbuf_t mbuf);

rs_ts_t
rs_ts_get_from_sock(int sockid);
#else
static inline void
rs_ts_pass_to_stream(mbuf_t mbuf, struct tcp_stream *cur_stream)
{}

static inline void
rs_ts_pass_from_stream(struct tcp_stream *cur_stream, mbuf_t mbuf)
{}

static inline rs_ts_t
rs_ts_get_from_sock(int sockid)
{
	return NULL;
}
#endif

/******************************************************************************/
#define MLOOP_ST_RCHECK		0 // begin to io_recv_check()
#define MLOOP_ST_RECV		1 // begin to process received packets
#define MLOOP_ST_TIMTOUT	2 // begin to check timeout lists
#define MLOOP_ST_APPTHREAD	3 // begin to switch to app thread
#define MLOOP_ST_APPCALL	4 // begin to handle_app_call()
#define MLOOP_ST_SEND		5 // begin to write_tcp_packets()
#define MLOOP_ST_SCHECK		6 // begin to write_tcp_packets()
struct mainloop_timestamp
{ 
#if MAINLOOP_TIMESTAMP
	#ifdef MLTS_SAMPLE
	uint32_t count;
	#endif
	uint64_t ts[7];
	double recv_time;
	double send_time;
	uint32_t recv_num;
	uint32_t send_num;
#endif
};
typedef struct mainloop_timestamp *ml_ts_t;
/******************************************************************************/
// mainloop stage delay estimate
static inline uint64_t
ml_ts_get_time()
{
	return get_abs_time_ns();
}


static inline void
ml_ts_add(ml_ts_t ts, uint8_t stage)
{
#if MAINLOOP_TIMESTAMP
	#ifdef MLTS_SAMPLE
	if (likely(ts->count)) {
		return;
	}
	#endif
	ts->ts[stage] = ml_ts_get_time();
#endif
}

static inline void
ml_ts_reset(ml_ts_t ts)
{
#if MAINLOOP_TIMESTAMP
	#ifdef MLTS_SAMPLE
	ts->count = 1;
	#endif
#endif
}

static inline void
ml_timeout_check(ml_ts_t ts, int core_id, uint64_t loop_num, int rcv_num, 
		int ctl_num, int rsp_num, int ack_num)
{
#if MAINLOOP_TIMESTAMP
	#ifdef MLTS_SAMPLE
	if (likely(ts->count)) {
		if (unlikely(ts->count == MLTS_SAMPLE_CYCLE - 1)) {
			ts->count = 0;
		} else {
			ts->count++;
		}
		return;
	} else {
		ts->count++;
	}
	#endif

	int snd_num = ctl_num + rsp_num + ack_num;
	if (rcv_num) {
		ts->recv_time += ts->ts[MLOOP_ST_TIMTOUT] - ts->ts[MLOOP_ST_RECV];
		ts->recv_num += rcv_num;
	}
	if (snd_num) {
		ts->send_time += ts->ts[MLOOP_ST_SCHECK] - ts->ts[MLOOP_ST_SEND];
		ts->send_num += snd_num;
	}
	if (rcv_num < MLTS_BATCH_THRESH && snd_num < MLTS_BATCH_THRESH) {
		return;
	}
	
	uint64_t end_ts = ml_ts_get_time();
	#ifndef MLTS_SAMPLE
	if (end_ts - ts->ts[0] > MAINLOOP_TIMEOUT_THRESH) 
	#endif
	{
		TRACE_TRACE("mainloop_timeout @ Stack %d @ loop %llu!\n"
				"rcv_num: %d, ctl_num: %d, rsp_num: %d, ack_num: %d\n"
				"full stage:\t%llu\n"
				"rece_check:\t\t%llu\n"
				"recv process:\t\t%llu\n"
				"timeout check:\t\t%llu\n"
				"app thread:\t\t%llu\n"
				"app call:\t\t%llu\n"
				"write pkt:\t\t%llu\n"
				"send_check:\t\t%llu\n"
				"average recv: %.2lf\t send: %.2lf\n"
				, core_id, loop_num
				, rcv_num, ctl_num, rsp_num, ack_num
				, end_ts - ts->ts[0]
				, ts->ts[MLOOP_ST_RECV] - ts->ts[MLOOP_ST_RCHECK]
				, ts->ts[MLOOP_ST_TIMTOUT] - ts->ts[MLOOP_ST_RECV]
				, ts->ts[MLOOP_ST_APPTHREAD] - ts->ts[MLOOP_ST_TIMTOUT]
				, ts->ts[MLOOP_ST_APPCALL] - ts->ts[MLOOP_ST_APPTHREAD]
				, ts->ts[MLOOP_ST_SEND] - ts->ts[MLOOP_ST_APPCALL]
				, ts->ts[MLOOP_ST_SCHECK] - ts->ts[MLOOP_ST_SEND]
				, end_ts - ts->ts[MLOOP_ST_SCHECK]
				, ts->recv_time/ts->recv_num, ts->send_time/ts->send_num
				);
	}
#endif
}
/******************************************************************************/
struct single_timestamp
{
	uint64_t start_time;	///< the start of timestamp
	uint32_t count;			///< the counter for sample
	uint32_t sum_count;		///< the count of sampled time intervals
	double time_sum;		///< the sum of sampled time intervals
};
typedef struct single_timestamp *single_ts_t;
extern struct single_timestamp single_ts[];
#define SINGLE_TS_CYCLE		100
#define SINGLE_TS_SAMPLE_AT_START
#define USING_SINGLE_TS_BARRIER

#ifdef USING_SINGLE_TS_BARRIER
	#define SINGLE_TS_BARRIER(f,m...)	_mm_lfence()
#else
	#define SINGLE_TS_BARRIER(f,m...)	(void)0
#endif

/**
 * The start of the single_timestamp test \n
 * Sampling the timestamp
 *
 * @param core_id	which core the function is called
 *
 * @return null
 */
static inline void
single_ts_start(int core_id)
{
	single_ts_t ts = &single_ts[core_id];
#ifdef SINGLE_TS_SAMPLE_AT_START
	ts->count++;
    printf("start count = %d, core_id = %d\n", ts->count, core_id);
#endif
	if (unlikely(ts->count == SINGLE_TS_CYCLE)) {
		SINGLE_TS_BARRIER();
		ts->start_time = get_abs_time_ns();
	}
}

/**
 * The end of single_timestamp test \n
 * Sampling the timestamp \n
 * print the duration of this sampled turn, the count of procedure excuted 
 * during this sampled turn, and the average cost of every procedure
 *
 * @param core_id	which core the function is called
 * @param count		the procedure executed during the single_ts test
 *
 * @return null
 */
static inline void
single_ts_end(int core_id, int count)
{
	uint64_t timeval;
	uint64_t cur_ts;
	single_ts_t ts = &single_ts[core_id];
    printf("ts->count = %d, core_id = %d\n", ts->count, core_id);
	if (unlikely(ts->count == SINGLE_TS_CYCLE)) {
		ts->count = 0;
		// if count is 0, is't meaningless and don't statistic or print
		if (count == 0)  { 
			return;
		}
		cur_ts = get_abs_time_ns();
		SINGLE_TS_BARRIER();
		timeval = cur_ts - ts->start_time;
		ts->time_sum += timeval;
		ts->sum_count += count;
		TRACE_TRACE("Core %d: timeval test: %llu ns, count: %d "
				"avg. %.2lf/%.2lf ns\n", 
				core_id, timeval, count, 
				(double)timeval/count,  ts->time_sum/ts->sum_count);
#ifndef SINGLE_TS_SAMPLE_AT_START
	} else {
		ts->count++;
#endif
	}
}
/******************************************************************************/
#define MBUF_TS_TIMEOUT		1000000000
static inline void
mbuf_ts_start(mbuf_t mbuf)
{
#if MBUF_PER_REQ_TS
	mbuf->mbuf_ts = get_time_ns();
#endif
}

static inline void
mbuf_ts_end(mbuf_t mbuf)
{
#if MBUF_PER_REQ_TS
	if (mbuf->mbuf_ts) {
		uint64_t interval = get_time_ns() - mbuf->mbuf_ts;
		if (interval > MBUF_TS_TIMEOUT) {
			TRACE_EXCP("mbuf ts test timeout! interval: %ld\n", interval);
		}
		mbuf->mbuf_ts = 0;
	} 
#endif
}

static inline void
mbuf_ts_pass(mbuf_t recv, mbuf_t send)
{
#if MBUF_PER_REQ_TS
	send->mbuf_ts = recv->mbuf_ts;
	recv->mbuf_ts = 0;
#endif
}
/******************************************************************************/
#endif //#ifdef __TIMESTAMP_H_
