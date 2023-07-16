/*
* mTCP source code is distributed under the Modified BSD Licence.
* 
* Copyright (C) 2015 EunYoung Jeong, Shinae Woo, Muhammad Jamshed, Haewon Jeong, 
* Sunghwan Ihm, Dongsu Han, KyoungSoo Park
* 
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
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

struct qepoll;
typedef struct qepoll *qepoll_t;

struct qapp_context;
typedef struct qapp_context *qapp_t;

typedef void *(*pfn_coroutine_t)( void * );

#include <stdint.h>
#include <stdbool.h>
#include <sys/queue.h>
#include "universal.h"

/** levels of prority*/
#define NUM_PRI 2
#define QEPOLL_SIZE 6000000
#define WHEEL_SIZE 1

#define MAX_CONCURR MAX_FLOW_NUM

#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#define QVDEB


/*----------------------------------------------------------------------------*/
enum qepoll_op
{
	Q_EPOLL_CTL_ADD = 1, 
	Q_EPOLL_CTL_DEL = 2, 
	Q_EPOLL_CTL_MOD = 3
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

enum qevent_dire
{
	STACK_TO_QE = 1, 
	APP_TO_QE   = 2
};

enum qevent_nature
{
	Q_READ = 0x001,
	Q_ACCEPT = 0x010, 
	Q_CLOSE  = 0x100
};

/*----------------------------------------------------------------------------*/

/**
 * Structure for storing qepoll event data.
 */
union qepoll_data
{
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
 };
typedef union qepoll_data qepoll_data_t;

/**
 * Structure for storing qepoll event infomation.
 */
struct qepoll_event
{
	uint8_t pri;
	uint8_t dire;
	uint8_t nature;
	uint8_t core;
	uint8_t apid;
	uint32_t events;
	uint32_t sockid;
	int timeout;
	qepoll_data_t data;
#ifdef QVDEB
	int      flow_st;
    uint64_t time_sp; 
#endif
	struct qepoll_event *next;
};

/**
 * Structure for storing event items with timout state.
 */
struct qevent_que
{
	volatile int start;			// starting index
	volatile int end;			// ending index
	
	volatile int size;					// max size
	volatile struct qepoll_event *qevent;	
};
typedef struct qevent_que *eque_t;

/**
 * Structure for storing event queues with prority.
 */
struct qtimer{
	eque_t qEvque[NUM_PRI]; 
	int qItemSize[NUM_PRI];
	volatile long long llStartIdx[NUM_PRI];
	
	int iItemSize;
	volatile unsigned long long ullStart;
};
typedef struct qtimer *qTimer_t;

struct chan{
	int id;
	int src;
	int dst;
	qTimer_t qTimeout;
};
typedef struct chan *chan_t;

/**
 * Structure for qepoll framework.
 */
struct qepoll{
	int qfd;
	int size;
	chan_t sq_chan;
	chan_t qs_chan;
	struct qepoll_event *result;
	//struct qepoll_stat stat;
}; 
typedef struct qepoll *qepoll_t;

/**
 * Structure for socket mapped to manager handler.
 */
struct socket_map
{
	int id;
	int sockid;
	int socktype;
	uint32_t opts;
	qepoll_t qe;

	volatile uint32_t qepoll;			/* registered events */
	volatile uint32_t qevents;		/* available events */
	qepoll_data_t     ep_data;

	TAILQ_ENTRY (socket_map) free_smap_link;
};
/*----------------------------------------------------------------------------*/
typedef struct socket_map * socket_map_t;


/**
 * Structure for qepoll manager handler.
 */
struct qepoll_manager
{
	/* variables related to event */
	qepoll_t qe;
	socket_map_t smap;
	chan_t sq_chan;
	chan_t qs_chan;
	uint32_t ts_last_event;
};
typedef struct qepoll_manager *qmag_t;

typedef struct qItem qTimewheelItem_t;

typedef void (*OnPreparePfn_t)( qTimewheelItem_t *, struct qepoll_event *);
typedef void (*OnProcessPfn_t)( qTimewheelItem_t * );

struct qItem
{
	unsigned long long ullExpireTime;

	OnPreparePfn_t pfnPrepare;
	OnProcessPfn_t pfnProcess;

	void *pArg; // routine 
	bool bTimeout;
};

struct qconfig_epoll
{
	int num_stack;
	int num_server;
	int num_app;
	int efd;
	socket_map_t g_smap;
	TAILQ_HEAD (, socket_map) free_smap;
};
typedef struct qconfig_epoll *qconfig_t;
extern qconfig_t qconf;

struct qevent_reg
{
	volatile int pr_core;
	volatile bool proc;
};

struct qarg{
	uint32_t qev_count;
	uint32_t app_count;
	struct qepoll_event *app_buff;
	struct qepoll_event *qevent_buff;
	qmag_t manager_buff;
	qTimewheelItem_t *qTimeItems_buff;
	qTimewheelItem_t *aTimeItems_buff;
};
typedef struct qarg *qarg_t;

extern qmag_t *g_qepoll;
extern qarg_t g_qarg, g_sarg;

extern volatile unsigned long long global_now;

uint64_t
GetTickMS();

/** 
 * initialize qepoll struct and allocate memory
 * @param[in]	 size  	max size of queue
 * @return		 efd of the event; -1 if the create operation fails
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int 
qepoll_create(uint32_t size);

/** 
 * wait for events in polling/coroutine mode
 * @param[in]	 efd  		qepoll id
 * @param[in]	 events  	qepoll events data structures
 * @param[in]	 maxevents  the maximum number of the events can be fetched
 * @param[in]	 timeout  	the timeout of qepoll wait for events
 * @return		 events number to be processed; -1 means timeout
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int 
qepoll_wait(int efd, struct qepoll_event *events, int maxevents, int timeout);

/**
 * wait epoll events with explicit priority
 *
 * @param[in]	 efd  		qepoll id
 * @param[out]	 events_h	the waited high-priority events
 * @param[out]	 events_l	the waited low-priority events
 * @param[in]	 maxevents  the maximum number of the events can be fetched
 * @param[out]	 high_num	the num of high-priority event ewaited out
 * @param[out]	 low_num	the num of low-priority event ewaited out
 * @param[in]	 timeout  	the timeout of qepoll wait for events
 * @return		 
 * 	return the total number of events to be processed; return -1 if timeout
 */
int 
qepoll_wait_pri(int efd, struct qepoll_event *events_h, 
		struct qepoll_event *events_l, int maxevents, int *high_num, 
		int *low_num, int timeout);

/** 
 * add, delete and modify events types
 * @param[in]	 efd		qepoll file descriptor id
 * @param[in]	 op 		the operation for modify events states
 * @param[in]	 sockid		socket id of the event
 * @param[in]	 events		qepoll events data structures
 * @param[in]	 allNow		the current timestamp in ms
 * @return		 events number to be processed; -1 means timeout
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int
qepoll_ctl(int efd, int op, int fd, struct qepoll_event *event, unsigned long long allNow);


/** 
 * add an event with prority and timeout into qepoll 
 * @param[in]	 event 			the event type
 * @param[in]	 allNow 		the current timestamp
 * @param[in]	 qevent 		qepoll event 
 * @return		 errno
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int
AddTimeEvent(uint32_t event,uint64_t allNow, struct qepoll_event *qevent);

/** 
 * generate an event with prority, core id and socket id
 * @param[in]	 pri		the event prority
 * @param[in]	 core 		the stack core id
 * @param[in]	 sockid		the event socket id 
 * @param[in]    dst        the server id
 * @param[in]    type       the event type
 							Q_READ  : read_event
							Q_CLOSE : close event 
                            Q_ACCEPT: accept event
 * @return		 qevent		qepoll event ptr
 * @ref 		 qepoll.h
 * @see
 * @note
 */

struct qepoll_event*
CreateQevent(uint8_t pri, uint8_t core, int sockid, int dst, uint8_t type);

int qepoll_reset(int app_id, int fd);

inline struct qevent_reg
get_ev_stat(int src, int sockid);

inline bool
EventQueueFull(eque_t eq);

struct qepoll_event*
q_alloc_epres(int num);

chan_t
qepoll_get_thread_chan(int stk_id, int srv_id);

void
qconf_init(int stack, int server, int app);

inline qconfig_t 
get_qconf();

void*
qstack_get();
 
/** 
 * initialize qepoll manager handler for allocating fd and map list
 * @return		 qepoll manager handler
 * @ref 		 qepoll.h
 * @see
 * @note
 */
void
q_init_manager(int stack, int server);
/******************************************************************************/
 // qepoll_event list
struct event_list
{
	volatile struct qepoll_event *head;
	volatile struct qepoll_event *tail;
};
typedef struct event_list *event_list_t;

static inline void
event_list_init(event_list_t list)
{
	list->head = NULL;
	list->tail = NULL;
}

static inline struct qepoll_event *
event_list_pop(event_list_t list)
{
	struct qepoll_event *ret;
	ret = list->head;
	if (ret) {
		list->head = ret->next;
		ret->next = NULL;
		if (!list->head) {
			list->tail = NULL;
		}
	}
	return ret;
}

static inline void
event_list_append(event_list_t list, struct qepoll_event *event)
{
	if (!list->tail) {
		list->head = event;
		list->tail = event;
		event->next = NULL;
	}
	else {
		list->tail->next = event;
		event->next = NULL;
		list->tail = event;
	}
}

/******************************************************************************/
 // tools for user explicitly get priority-based events
struct evmgt_queue
{
	struct qepoll_event *events;
	int num;
	int p;
};
typedef struct evmgt_queue *evmgtq_t;

struct event_mgt
{
	int size;	// max size of event queue
	int lb;
	struct evmgt_queue events_h;
	struct evmgt_queue events_l;
};
typedef struct event_mgt *evmgt_t;

void 
eventmgt_init(evmgt_t evmgt, int maxsize, int lb);

struct qepoll_event*
eventmgt_get(int efd, struct event_mgt *mgt);
/******************************************************************************/
#endif //#ifndef __QEPOLL_H_
