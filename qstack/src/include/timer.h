/**
 * @file timer.h
 * @brief tcp timers
 * @author Feng Xiao(fengxiao@ict.ac.cn) Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.6.9
 * @version 1.0
 * @note all the timers use the timestamp with millisecond
 * @detail Function list: \n
 *   1. timer_init(): init the timers\n
 *   2. rto_list_add(): add tcp_stream to rto_list\n
 *   3. rto_list_remove(): remove tcp_stream from rto_list\n
 *   4. rto_list_update(): update tcp_stream on the rto_list\n
 *   5. rto_list_check(): walk the rto_list to check any timeout\n
 *   6. timewait_list_add(): add tcp_stream to timewait_list\n
 *   7. timewait_list_remove(): remove tcp_stream from timewait_list\n
 *   8. timewait_list_update(): update tcp_stream on the timewait_list\n
 *   9. timewait_list_check(): walk the timewait_list to check any timeout\n
 *   10. timeout_list_add(): add tcp_stream to timeout_list\n
 *   11. timeout_list_remove(): remove tcp_stream from timeout_list\n
 *   12. timeout_list_update(): update tcp_stream on the timeout_list\n
 *   13. timeout_list_check(): walk the timeout_list to check any timeout\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.6.9
 *   	Author: Shen Yifan
 *   	Modification: add comments
 *   2. Date: 2019.6.9
 *   	Author: Shen Yifan
 *   	Modification: change to inline mode
 *   3. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef _TIMER_H
#define _TIMER_H
/******************************************************************************/
#include "universal.h"
#include <stdint.h>
#include <sys/time.h>
#include "tcp_out.h"
/******************************************************************************/
// global macros
#define TCP_TIMEWAIT			(MSEC_TO_USEC(5000) / TIME_TICK)	// 5s
#define TCP_INITIAL_RTO			(MSEC_TO_USEC(500) / TIME_TICK)		// 500ms
//#define TCP_FIN_RTO				(MSEC_TO_USEC(500) / TIME_TICK)		// 500ms
#define TCP_TIMEOUT				(MSEC_TO_USEC(300000) / TIME_TICK)	// 300s
/*----------------------------------------------------------------------------*/
#define RTO_STORAGE 		3000	// default RTO queue length
/******************************************************************************/
// Psudo data structures for testing requirements
#ifndef TCP_SIMPLE_RTO
struct rto_hashstore 
{
  uint32_t rto_now_idx;
  uint32_t rto_now_ts;
  TAILQ_HEAD(rto_head, tcp_stream) rto_list[RTO_STORAGE+1];
};
#endif
/******************************************************************************/
/**
 * remove the stream from rto_list
 *
 * @param qstack 		stack process context
 * @param cur_stream		target tcp_stream
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline void 
handle_rto(qstack_t qstack, uint32_t cur_ts, tcp_stream_t cur_stream) 
{
	TRACE_TIMER("stream: %u, cur_ts: %u, rto ts: %u, time delta: %d\n",
			cur_stream->id, cur_ts, cur_stream->ts_rto,
			(int32_t)(cur_ts - cur_stream->ts_rto));
	if (cur_stream->sndvar.nrtx < TCP_MAX_RTX) {
		cur_stream->sndvar.nrtx++;
	} else {
		/* if it exceeds the threshold, destroy and notify to application */
		TRACE_EXCP("Exceed MAX_RTX @ Stream %d\n", cur_stream->id);
		if (cur_stream->state < TCP_ST_ESTABLISHED) {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_CONN_FAIL;
          	tcp_stream_destroy(qstack, cur_stream);
		} else {
			cur_stream->state = TCP_ST_CLOSED;
			cur_stream->close_reason = TCP_CONN_LOST;
//			if (cur_stream->socket) {
//				RaiseErrorEvent(mtcp, cur_stream);
//			} else {
          		tcp_stream_destroy(qstack, cur_stream);
//			}
		}
		return ERROR;
	}
	// TODO: estimate rto with timestamp
	cur_stream->sndvar.rto = TCP_INITIAL_RTO << cur_stream->sndvar.nrtx;

	// update ssthreash and cwnd
	// TODO: dynamicall choose congestion algorithm
	cur_stream->sndvar.ssthresh = MIN(cur_stream->sndvar.cwnd, 
			cur_stream->sndvar.peer_wnd) / 2;
	if (cur_stream->sndvar.ssthresh < (2 * cur_stream->sndvar.mss)) {
		cur_stream->sndvar.ssthresh = cur_stream->sndvar.mss * 2;
	}
	cur_stream->sndvar.cwnd = cur_stream->sndvar.mss;

	// handle retransmission
	cur_stream->snd_nxt = cur_stream->sndvar.snd_una;
	if (cur_stream->state == TCP_ST_ESTABLISHED) {
		// reset retrans_queue add to send queue
		sb_reset_retrans(&cur_stream->sndvar.sndbuf);
		send_event_enqueue(&qstack->send_event_queue, qstack->stack_id, 
				cur_stream);
	} else if (cur_stream->state == TCP_ST_FIN_WAIT_1 || 
			cur_stream->state == TCP_ST_CLOSING ||
			cur_stream->state == TCP_ST_LAST_ACK) {
		if (cur_stream->sndvar.fss == 0) {
			TRACE_ERROR("fss not set @ Stream %d\n", cur_stream->id);
		} 
		if (TCP_SEQ_LT(cur_stream->snd_nxt, cur_stream->sndvar.fss)) {
			// retrans data first
			sb_reset_retrans(&cur_stream->sndvar.sndbuf);
			send_event_enqueue(&qstack->send_event_queue, qstack->stack_id, 
					cur_stream);
		} else {
			control_queue_add(qstack, cur_stream);
		}
	} else {
		control_queue_add(qstack, cur_stream);
	}
}

static inline void 
adjust_backup_store(qstack_t qstack) 
{
#if TCP_TIMER_RTO
	#ifndef TCP_SIMPLE_RTO
  tcp_stream_t walk, next;
  struct rto_head *rto_list = &qstack->rto_store->rto_list[RTO_STORAGE];
  for (walk = TAILQ_FIRST(rto_list); walk != NULL; walk = next) {
    next = TAILQ_NEXT(walk, timer_link);
    int diff = (int32_t)(walk->ts_rto - qstack->rto_store->rto_now_ts);
    if (diff < RTO_STORAGE) {
      int offset = (diff + qstack->rto_store->rto_now_idx) % RTO_STORAGE;
      TAILQ_REMOVE(&qstack->rto_store->rto_list[RTO_STORAGE], walk, timer_link);
      walk->on_rto_idx = offset;
      TAILQ_INSERT_TAIL(&qstack->rto_store->rto_list[offset], walk, timer_link);
    }
  }
	#endif
#endif
}
/******************************************************************************/
/**
 * add the tcp_stream into rto_list
 * 
 * @param qstack 		stack process context
 * @param cur_stream 	target tcp_stream 
 * @param cur_ts		current time (ms)
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
rto_list_add(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts) 
{
#if TCP_TIMER_RTO
  #ifndef TCP_SIMPLE_RTO
  if (!qstack->rto_list_cnt) {
    qstack->rto_store->rto_now_idx = 0;
    qstack->rto_store->rto_now_ts = cur_stream->ts_rto;
  }

  if (cur_stream->on_rto_idx < 0) {
    if (cur_stream->on_timewait_list) {
      TRACE_EXCP("rto_list_add failed: "
			  "can not be both in rto and timewait list @ Stream %d\n",
              cur_stream->id);
      return 1;
    }

  	cur_stream->ts_rto = cur_ts + cur_stream->sndvar.rto;
    int diff = (int32_t)(cur_stream->ts_rto - qstack->rto_store->rto_now_ts);
    if (diff < RTO_STORAGE) {
      int offset = (diff + qstack->rto_store->rto_now_idx) % RTO_STORAGE;
      cur_stream->on_rto_idx = offset;
      TAILQ_INSERT_TAIL(&(qstack->rto_store->rto_list[offset]), cur_stream,
                        timer_link);
    } else {
      cur_stream->on_rto_idx = RTO_STORAGE;
      TAILQ_INSERT_TAIL(&(qstack->rto_store->rto_list[RTO_STORAGE]), cur_stream,
                        timer_link);
    }
    qstack->rto_list_cnt++;
  }
  #else //#ifndef TCP_SIMPLE_RTO
  if (cur_stream->on_rto_idx == 0) {
  	cur_stream->ts_rto = cur_ts + cur_stream->sndvar.rto;
    if (!cur_stream->on_timewait_list) {
      cur_stream->on_rto_idx = RTO_STORAGE;
      TAILQ_INSERT_TAIL(&qstack->rto_list, cur_stream, timer_link);
    } else {
      TRACE_EXCP("rto_list_add failed: "
			  "can not be both in rto and timewait list @ Stream %d\n",
              cur_stream->id);
      return 1;
	}
  } else {
    return 1;
  }
  #endif //#ifndef TCP_SIMPLE_RTO
#endif //#if TCP_TIMER_RTO
  return 0;
}

/**
 * remove the stream from rto_list
 *
 * @param qstack 		stack process context
 * @param cur_stream		target tcp_stream
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
rto_list_remove(qstack_t qstack, tcp_stream_t cur_stream) 
{
#if TCP_TIMER_RTO
  #ifndef TCP_SIMPLE_RTO
  if (cur_stream->on_rto_idx < 0) {
    TRACE_TIMER("failed to delete from rte list: not on rto list @ Stream %d\n",
            cur_stream->id);
    return 1;
  }
  TAILQ_REMOVE(&qstack->rto_store->rto_list[cur_stream->on_rto_idx], cur_stream,
               timer_link);
  cur_stream->on_rto_idx = -1;
  qstack->rto_list_cnt--;
  #else //#ifndef TCP_SIMPLE_RTO
  if (cur_stream->on_rto_idx) {
    TAILQ_REMOVE(&qstack->rto_list, cur_stream, timer_link);
	cur_stream->on_rto_idx = 0;
  } else {
    TRACE_TIMER("failed to delete from rte list: not on rto list @ Stream %d\n",
            cur_stream->id);
    return 1;
  }
  #endif //#ifndef TCP_SIMPLE_RTO
  cur_stream->sndvar.nrtx = 0;
#endif
  return 0;
}

/**
 * update the tcp_stream on the rto_list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 * @param cur_ts		current time (ms)
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline void 
rto_list_update(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
#if TCP_TIMER_RTO
//  assert(cur_stream->ts_rto > 0);
  rto_list_remove(qstack, cur_stream);
  rto_list_add(qstack, cur_stream, cur_ts);
#endif
}

/**
 * walk the rto list to check if there is any timeout
 *
 * @param qstack 	stack process context
 * @param cur_ts	current time (ms)
 *
 * @return
 * 	0 if there is no timeout event\n
 * 	cnt number of timeout events that has been taken care of
 */
static inline int 
rto_list_check(qstack_t qstack, uint32_t cur_ts) 
{
#if TCP_TIMER_RTO
  #ifndef TCP_SIMPLE_RTO
  tcp_stream_t walk, next;
  struct rto_head *rto_list;
  int cnt = 0;

  if (!qstack->rto_list_cnt) {
    return 0;
  }
  while (1) {
    rto_list = &qstack->rto_store->rto_list[qstack->rto_store->rto_now_idx];
    if ((int32_t)(cur_ts - qstack->rto_store->rto_now_ts) < 0) {
      break;
    }

    for (walk = TAILQ_FIRST(rto_list); walk != NULL; walk = next) {
      next = TAILQ_NEXT(walk, timer_link);
      if (walk->on_rto_idx >= 0) {
		TRACE_TIMER("rto happeded @ Stream %d\n", walk->id);
        TAILQ_REMOVE(rto_list, walk, timer_link);
        qstack->rto_list_cnt--;
        walk->on_rto_idx = -1;
        handle_rto(qstack, cur_ts, walk);
        cnt++;
      } else {
        TRACE_EXCP("not on rto list but walked @ Stream %d\n", walk->id);
      }
    }

    qstack->rto_store->rto_now_idx =
        (qstack->rto_store->rto_now_idx + 1) % RTO_STORAGE;
    qstack->rto_store->rto_now_ts++;
    if (!(qstack->rto_store->rto_now_ts % 1000)) {
      adjust_backup_store(qstack);
    }
  }
  return cnt;
  #else //#ifndef TCP_SIMPLE_RTO
  tcp_stream_t walk, next;
  int cnt = 0;
  for (walk = TAILQ_FIRST(&qstack->rto_list); 
		  (walk != NULL) && (cnt<MAX_RTO_BATCH);
		  walk = next) {
    if ((int32_t)(cur_ts - walk->ts_rto) < 0) {
      break;
	} else {
      cnt++;
      next = TAILQ_NEXT(walk, timer_link);
	  TRACE_TIMER("connection timeout happed @ Stream %d\n", walk->id);
      walk->on_rto_idx = 0;
      TAILQ_REMOVE(&qstack->rto_list, walk, timer_link);
      handle_rto(qstack, cur_ts, walk);
	}
  }
  return cnt;
  #endif //#ifndef TCP_SIMPLE_RTO
#else
  return 0;
#endif
}
/******************************************************************************/
/**
 * add the tcp_stream to timewait list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 * @param cur_ts		current time (ms)
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
timewait_list_add(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
#if TCP_TIMER_TIMEWAIT
  cur_stream->ts_tw_expire = cur_ts + TCP_TIMEWAIT;
  if (cur_stream->on_timewait_list) {
    TAILQ_REMOVE(&qstack->timewait_list, cur_stream, timer_link);
    TAILQ_INSERT_TAIL(&qstack->timewait_list, cur_stream, timer_link);
  } else {
    if (cur_stream->on_rto_idx >= 0) {
      TRACE_TIMER("can not be both in rto and timewait list @ Stream %d\n",
              cur_stream->id);
	  rto_list_remove(qstack, cur_stream);
    }
    cur_stream->on_timewait_list = TRUE;
    TAILQ_INSERT_TAIL(&qstack->timewait_list, cur_stream, timer_link);
  }
#endif
  return 0;
}

/**
 * remove the tcp_stream from timewait_list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
timewait_list_remove(qstack_t qstack, tcp_stream_t cur_stream) 
{
#if TCP_TIMER_TIMEWAIT
  if (!cur_stream->on_timewait_list) {
    TRACE_EXCP("failed to delete from timewait_list:"
			" not on timewait list @ Stream %d\n",
            cur_stream->id);
    return 1;
  }
  TAILQ_REMOVE(&qstack->timewait_list, cur_stream, timer_link);
  cur_stream->on_timewait_list = FALSE;
#endif
  return 0;
}

/**
 * walk the timewait list to check if there is any timeout
 *
 * @param qstack 	stack process context
 * @param cur_ts	current time (ms)
 *
 * @return
 * 	0 if there is no timeout event\n
 * 	cnt number of timeout events that has been taken care of
 */
static inline int 
timewait_list_check(qstack_t qstack, uint32_t cur_ts) 
{
#if TCP_TIMER_TIMEWAIT
  tcp_stream_t walk, next;
  int cnt = 0;
  for (walk = TAILQ_FIRST(&qstack->timewait_list); walk != NULL; walk = next) {
    if (walk->on_timewait_list) {
      if ((int32_t)(cur_ts - walk->ts_tw_expire) < 0) {
		  break;
	  } else {
        next = TAILQ_NEXT(walk, timer_link);
        cnt++;
		TRACE_TIMER("timewait timeout happened @ Stream %d\n", walk->id);
        walk->on_timewait_list = FALSE;
        TAILQ_REMOVE(&qstack->timewait_list, walk, timer_link);
        walk->state = TCP_ST_CLOSED;
        walk->close_reason = TCP_ACTIVE_CLOSE;
//        if (walk->socket) {
          // TODO: support error event
//          RaiseErrorEvent(mtcp, walk);
//        } else {
          tcp_stream_destroy(qstack, walk);
//        }
	  }
    } else {
      next = TAILQ_NEXT(walk, timer_link);
      TAILQ_REMOVE(&qstack->timewait_list, walk, timer_link);
	}
  }
  return cnt;
#else
  return 0;
#endif 
}
/******************************************************************************/
/**
 * add tcp_stream to timeout list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 * @param cur_ts		current time (ms)
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
timeout_list_add(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts) 
{
#if TCP_TIMER_TIMEOUT
  cur_stream->last_active_ts = cur_ts;
  if (!cur_stream->on_timeout_list) {
    cur_stream->on_timeout_list = TRUE;
    TAILQ_INSERT_TAIL(&qstack->timeout_list, cur_stream, timeout_link);
    return 0;
  } else {
    TRACE_EXCP("timeout_list_add failed: already on timeout list @ Stream %d\n",
            cur_stream->id);
    return 1;
  }
#else
  return 0;
#endif
}

/**
 * remove the stream from timeout_list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int 
timeout_list_remove(qstack_t qstack, tcp_stream_t cur_stream) 
{
#if TCP_TIMER_TIMEOUT
  if (!cur_stream->on_timeout_list) {
    TRACE_EXCP("failed to delete from timeout_list: "
			"not on timeout list @ Stream %d\n",
            cur_stream->id);
    return 1;
  }
  TAILQ_REMOVE(&qstack->timeout_list, cur_stream, timeout_link);
  cur_stream->on_timeout_list = FALSE;
#endif
  return 0;
}

/**
 * update the tcp_stream on the timeout list
 *
 * @param qstack 		stack process context
 * @param cur_stream	target tcp_stream
 * @param cur_ts		current time (ms)
 *
 * @return
 * 	1 if failed; 0 if succeeded
 */
static inline int
timeout_list_update(qstack_t qstack, tcp_stream_t cur_stream, uint32_t cur_ts)
{
#if TCP_TIMER_TIMEOUT
  cur_stream->last_active_ts = cur_ts;
  if (cur_stream->on_timeout_list) {
    TAILQ_REMOVE(&qstack->timeout_list, cur_stream, timeout_link);
    TAILQ_INSERT_TAIL(&qstack->timeout_list, cur_stream, timeout_link);
  }
#endif
  return 0;
}

/**
 * walk the timeout list to check if there is any timeout
 *
 * @param qstack 	stack process context
 * @param cur_ts	current time (ms)
 *
 * @return
 * 	0 if there is no timeout event\n
 * 	cnt number of timeout events that has been taken care of
 */
static inline int 
timeout_list_check(qstack_t qstack, uint32_t cur_ts) 
{
#if TCP_TIMER_TIMEOUT
  tcp_stream_t walk, next;
  int cnt = 0;
  for (walk = TAILQ_FIRST(&qstack->timeout_list); walk != NULL; walk = next) {
    if ((int32_t)(cur_ts - walk->last_active_ts) < TCP_TIMEOUT) {
		break;
	} else {
      next = TAILQ_NEXT(walk, timeout_link);
      cnt++;
	  TRACE_TIMER("connection timeout happed @ Stream %d\n", walk->id);
      walk->on_timeout_list = FALSE;
      TAILQ_REMOVE(&qstack->timeout_list, walk, timeout_link);
      walk->state = TCP_ST_CLOSED;
      walk->close_reason = TCP_TIMEOUT;
	  TRACE_CLOSE("destroy tcp stream due to timeout @ Stream %d\n", walk->id);
//      if (walk->socket) {
		  // TODO: support error event
//        RaiseErrorEvent(mtcp, walk);
//      } else {
        tcp_stream_destroy(qstack, walk);
//      }
	}
  }
  return cnt;
#else
  return 0;
#endif
}
/******************************************************************************/
// function declearation
/**
 * init the timer structure
 *
 * @param qstack 	stack process context
 *
 * @return
 * 	0 if succed; 1 if failed
 *
 * @note 
 * 	must be called during the initialization of qstack
 */
static inline int 
timer_init(qstack_t qstack) 
{
#if TCP_TIMER_RTO
  #ifndef TCP_SIMPLE_RTO
  int i;
  struct rto_hashstore *hs = calloc(1, sizeof(struct rto_hashstore));
  TRACE_CHECKP("init timer @ Stack %d\n", qstack->stack_id);
  if (!hs) {
    TRACE_ERR("failed to calloc rto_hashstore\n");
    return 1;
  }
  for (i = 0; i <= RTO_STORAGE; i++) {
    TAILQ_INIT(&hs->rto_list[i]);
  }
  qstack->rto_store = hs;
  #else
  TAILQ_INIT(&qstack->rto_list);
  #endif
#endif
#if TCP_TIMER_TIMEWAIT
  TAILQ_INIT(&qstack->timewait_list);
#endif
#if TCP_TIMER_TIMEOUT
  TAILQ_INIT(&qstack->timeout_list);
#endif
  return 0;
}
/******************************************************************************/
#endif //#ifndef _TIMER_H
