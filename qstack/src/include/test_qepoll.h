/**
* @file  qepoll.h  
* @brief event-driven framework based on epoll and timing wheel
* @author       Song Hui
* @date     2018-7-20 
* @version  V1.0 
* @copyright Song Hui                                                              
*/
#ifndef __QEPOLL_H_
#define __QEPOLL_H_

#include <stdint.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "circular_queue.h"

/** levels of prority*/
#define NUM_PRI 2
#define QEPOLL_SIZE 1024*10

#define MAX_CONCURR 3000000

typedef void *(*pfn_coroutine_t)( void * );

/*----------------------------------------------------------------------------*/
enum qepoll_op
{
	Q_EPOLL_CTL_ADD = 1, 
	Q_EPOLL_CTL_DEL = 2, 
	Q_EPOLL_CTL_MOD = 3, 
};
/*----------------------------------------------------------------------------*/
enum qevent_type
{
	Q_EPOLLNONE	= 0x000, 
	Q_EPOLLIN	= 0x001, 
	Q_EPOLLPRI	= 0x002,
	Q_EPOLLOUT	= 0x004,
	Q_EPOLLRDNORM	= 0x040, 
	Q_EPOLLRDBAND	= 0x080, 
	Q_EPOLLWRNORM	= 0x100, 
	Q_EPOLLWRBAND	= 0x200, 
	Q_EPOLLMSG		= 0x400, 
	Q_EPOLLERR		= 0x008,
	Q_EPOLLHUP		= 0x010,
	Q_EPOLLRDHUP 	= 0x2000,
	Q_EPOLLONESHOT	= (1 << 30), 
	Q_EPOLLET		= (1 << 31)
};
/*----------------------------------------------------------------------------*/


/**
 * Structure for storing qepoll event data.
 */
union qepoll_data{
	void *ptr;
	int sockid;
	uint32_t u32;
	uint64_t u64;
};
typedef union qepoll_data qepoll_data_t;

/**
 * Structure for storing qepoll event infomation.
 */
struct qepoll_event{
	int sockid;
	uint32_t events;
	qepoll_data_t data;
	int pri;
};

typedef struct circular_queue qepoll;
typedef qepoll *qepoll_t;

static inline void 
qepoll_init(qepoll_t ep)
{
	cirq_init(ep, QEPOLL_SIZE);
}

// remember to free the event after using!
static inline struct qepoll_event *
qepoll_get(qepoll_t ep)
{
	return (struct qepoll_event *)cirq_get(ep);
}

static int
qepoll_add(qepoll_t ep, struct qepoll_event *event)
{
	return cirq_add(ep, (void*)event);
}
#endif //#ifndef __QEPOLL_H_

