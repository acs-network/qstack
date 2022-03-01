/*
* Qepoll source code is distributed under the Qstack socket interface.
*
* Qepoll is an event-driven framework with multi-pri queues and distributed events across multi-core for qstack 
* Based on api.c code.
*
* Author Hui Song
*/
/**
 * @file: qepoll.c
 * @author: Hui Song 
 * @date 2018.8.19 
*/
#include "qstack.h"
#include "qepoll.h"
#include "debug.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <emmintrin.h>

#include <sys/time.h>
#include <errno.h>
	 
#include <assert.h>
#include <time.h>

qstack_t qstack;
qconfig_t qconf;

qmag_t *g_qepoll;
qarg_t g_qarg, g_sarg;
volatile unsigned long long global_now;
//int errno;
int core_idx;

extern ts_t ts_system_init; 

struct qevent_reg reg[MAX_FLOW_NUM];

#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL TRACELV_DETAIL
#endif

//#define TRACE_CTIME
#ifdef TRACE_CTIME
    #define TRACE_EPOLLTIME(f,m...) TRACE_FILEOUT(f,##m)
	struct timespec ts_s,ts_e;
#else
    #define TRACE_EPOLLTIME(f,m...) (void)0
#endif

/** 
 * get the current timestamp represented in ms
 * @return		 current timestamp in ms
 * @ref 		 qepoll.h
 * @see
 * @note
 */
uint64_t
GetTickMS()
{
#if defined( __LIBCO_RDTSCP__) 
	static uint32_t khz = getCpuKhz();
	return counter() / khz;
#else
/*	struct timespec now = { 0 };
	clock_gettime(CLOCK_MONOTONIC, &now);
	unsigned long long u = now.tv_sec;
	u *= 1000000;
	u += now.tv_nsec / 1000;
	return u;
*/
	return get_time_ms();
#endif

}

inline bool EventQueueUsed(eque_t eq)
{
	return eq->end != eq->start;	
}

inline bool EventQueueFull(eque_t eq)
{
	return ((eq->end + 1) % eq->size) == eq->start;	
}

/** 
 * initialize event queues and allocate memory
 * @param[in]	 size  	max size of queue
 * @return		 event queue structure
 * @ref 		 qepoll.h
 * @see
 * @note
 */
eque_t  
q_create_eque(int isize, int qsize)
{
	int i;
	eque_t  eq;
	eq = (eque_t)calloc(qsize, sizeof(struct qevent_que));
	if (!eq)
		return NULL;
	for(i = 0;i < qsize;i++){
		eq[i].size = isize;
		eq[i].qevent = (struct qepoll_event *)
				calloc(isize, sizeof(struct qepoll_event));
		if (!eq[i].qevent) {
			free(eq+i);
			return NULL;
		}
	}

	return eq;	
}

struct qepoll_event *
q_alloc_epres( int n )
{
	struct qepoll_event *events = (struct qepoll_event*)calloc( 1,n * sizeof( struct qepoll_event ) );

	return events;
}

/** 
 * initialize qepoll to socket map and allocate memory
 * @return       socket map structure
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int
AllocateSocket(socket_map_t socket, int socktype)
{
#if 0
	socket_map_t socket = NULL;

	while (socket == NULL) {
		socket = TAILQ_FIRST(&qconf->free_smap);
		if (!socket) {
			TRACE_ERROR("The concurrent sockets are at maximum.\n");
			return NULL;
		}

		TAILQ_REMOVE(&qmag->free_smap, socket, free_smap_link);

		/* if there is not invalidated events, insert the socket to the end */
		/* and find another socket in the free smap list */
		if (socket->qevents) {
			TRACE_EPOLL("There are still not invalidate events remaining.\n");
			TAILQ_INSERT_TAIL(&qmag->free_smap, socket, free_smap_link);
			socket = NULL;
		}
	}
#endif
	if (!socket) {
		TRACE_ERROR("The concurrent sockets are at maximum.\n");
		return NULL;
	}

	socket->socktype = socktype;
	socket->opts = 0;
	socket->qepoll = 0;
	socket->qevents = 0;

	/* 
	 * reset a few fields (needed for client socket) 
	 * addr = INADDR_ANY, port = INPORT_ANY
	 */
	memset(&socket->ep_data, 0, sizeof(qepoll_data_t));

	return 0;
}


/** 
 * free qepoll socket structure memory
 * @param[in]	 qmag  	qepoll manager handler
 * @param[in]	 sockid  qepoll fd
 * @return		 void
 * @ref 		 qepoll.h
 * @see
 * @note
 */
void 
q_free_map(int sockid)
{
	socket_map_t socket = &qconf->g_smap[sockid];

	socket->qepoll = Q_EPOLLNONE;
	
	socket->socktype = 1;
	socket->opts = 0;
	socket->qepoll = 0;
	socket->qevents = 0;

	/* insert into free stream map */
	//TAILQ_INSERT_TAIL(&qconf->free_smap, socket, free_smap_link);
}

/** 
 * initialize qepoll timewheel structure and allocate memory
 * @param[in]	 iSize  	max size of timewheel,slots * time scale
 * @param[in]	 qSize  	max size of queue
 * @return		 qepoll timewheel structure
 * @ref 		 qepoll.h
 * @see
 * @note
 */
inline qTimer_t 
q_alloc_timeout( int iSize ,int qSize )
{
	int i;
	qTimer_t lp = (qTimer_t )calloc( 1,sizeof(struct qtimer) );	

	lp->iItemSize = qSize;

	lp->ullStart = 0;//GetTickMS();

	for( i = 0; i < NUM_PRI; i++){
		lp->qEvque[i] = q_create_eque(iSize,qSize);
		lp->qItemSize[i] = qSize;
		lp->llStartIdx[i] = 0;
	}

	return lp;
}

inline void 
q_take_timeout( qTimer_t apTimeout,unsigned long long allNow,struct qepoll_event *apResult,int pri,int maxevents,int *ev_cnt)
{
	int i,idx,j,num_events;
	eque_t  qItem;
	
	if( apTimeout->ullStart == 0 )
	{
		apTimeout->ullStart = allNow;
		apTimeout->llStartIdx[pri] = 0;
	}

	if( allNow < apTimeout->ullStart )
	{
		return ;
	}
	int cnt = allNow - apTimeout->ullStart + 1;
	if( cnt > apTimeout->qItemSize[pri] )
	{
		cnt = apTimeout->qItemSize[pri];
	}
	if( cnt < 0 )
	{
		return;
	}

	for( i = 0;i < cnt;i++)
	{
		idx = ( apTimeout->llStartIdx[pri] + i) % apTimeout->qItemSize[pri];
		qItem = apTimeout->qEvque[pri] + idx;
		for(j = 0; j < num_events && *ev_cnt < maxevents;j++){
			apResult[(*ev_cnt)++] = qItem->qevent[qItem->start];
			qItem->start++;
			if(qItem->start > qItem->size)
				qItem->start = 0;
		}
	}
	apTimeout->ullStart = allNow;
	apTimeout->llStartIdx[pri] += cnt - 1;
}

int qepoll_ctl(int app_id, int fd, int op, unsigned long long allNow, struct qepoll_event *event)
{
	uint8_t i;
	qmag_t qmag;
	qepoll_t qe;
	socket_map_t socket, socket_other;
	uint32_t events;
	struct qepoll_event *pend_ev;

	qmag = g_qepoll[app_id];
	
	/*if (fd < 0 || fd >= MAX_CONCURR * (app_id  + 1)) {
		TRACE_EXCP("@App %d Epoll id %d out of range.\n", app_id, fd);
		errno = EBADF;
		return -1;
	}*/

	
	if (!qmag) {
		TRACE_EXCP("Epoll manager doesn't exist at core id %d!", app_id);
		return -1;
	}

	if (fd < 0) {
		TRACE_EXCP("Epoll id %d out of range.\n", fd);
		errno = EBADF;
		return -1;
	}
	
	if (!qmag->qe || (!event && op != Q_EPOLL_CTL_DEL)) {
		TRACE_EXCP("Epoll event to be add or mod is NULL!\n");
		errno = EINVAL;
		return -1;
	}

	socket = qconf->g_smap + fd;

	if (op == Q_EPOLL_CTL_ADD) {
		/*if (qepoll_map->qepoll) {
			errno = EEXIST;
			return -1;
		}*/

		event->sockid = fd;
		event->apid   = app_id;
		
		/* EPOLLERR and EPOLLHUP are registered as default */
		events = event->events;
		events |= (Q_EPOLLERR | Q_EPOLLHUP);
		socket->ep_data = event->data;
		socket->qepoll  = events;
		
		TRACE_EPOLL("Adding epoll socket %d ET: %u, IN: %u, OUT: %u\n", 
				socket->id, socket->qepoll & Q_EPOLLET, 
				socket->qepoll & Q_EPOLLIN, socket->qepoll & Q_EPOLLOUT);	
		
		if (socket->socktype > 1 && fd > 0) {
			if (socket->qepoll & Q_EPOLLIN) {
				AddTimeEvent(Q_EPOLLIN, allNow, event);
			}
			if (socket->qepoll & Q_EPOLLOUT) {
#ifdef TRACE_CTIME
				clock_gettime(CLOCK_MONOTONIC_RAW,&ts_s);
#endif
				AddTimeEvent(Q_EPOLLOUT, allNow, event);
#ifdef TRACE_CTIME
				clock_gettime(CLOCK_MONOTONIC_RAW,&ts_e);
#endif
				TRACE_EPOLLTIME("Adding event cost %d ns\n",ts_e.tv_nsec-ts_s.tv_nsec);
			}
		}else{
			//qe = get_qe_from_socket(fd);
			if(bind_socket_with_qe(fd, qmag->qe) == 0){
				TRACE_EXCP("Bind qepoll with socket failed!\n");
			}
			pend_ev = get_pending_event(fd);
			if(pend_ev){
				pend_ev->dire = APP_TO_QE;
				pend_ev->apid = app_id; 
				AddTimeEvent(Q_EPOLLIN, allNow, pend_ev);
			}
			socket->socktype = 2;
			//TRACE_EXCP("socket:%p, id:%d, data.ptr:%p is reg!\n", socket, fd, socket->ep_data.ptr);
		}
	} else if (op == Q_EPOLL_CTL_MOD) {
		/*if (!qepoll_map->qepoll) {
			errno = ENOENT;
			return -1;
		}*/
		
		if(event == NULL){
			TRACE_EXCP("Epoll event to be mod is NULL!\n");
			return -1;
		}

		events = event->events;
		events |= (Q_EPOLLERR | Q_EPOLLHUP);
		socket->ep_data = event->data;
		socket->qepoll  = events;

		if (socket->socktype == SOCK_STREAM) {
			if (socket->qepoll & Q_EPOLLIN) {
				AddTimeEvent(Q_EPOLLIN, allNow, event);
			}
			if (socket->qepoll & Q_EPOLLOUT) {				
				AddTimeEvent(Q_EPOLLOUT, allNow, event);
				TRACE_EPOLLTIME("Adding event cost %d ns\n",ts_e.tv_nsec-ts_s.tv_nsec);
			}
		}

	} else if (op == Q_EPOLL_CTL_DEL) {

		TRACE_CLOSE("Del QEPOLL event from %d, socket %d start.\n",
					app_id, fd);
		if (!socket->qepoll) {
			errno = ENOENT;
			return -1;
		}

		socket->qepoll = Q_EPOLLNONE;
		
		TRACE_CLOSE("Del QEPOLL event from %d, socket %d done! \n",
					app_id, fd);
	}

	return 0;
	
}

int qepoll_reset(int app_id, int fd)
{
	qmag_t qmag;
	qmag = g_qepoll[app_id];
	q_free_map(fd);
	return 0;
}

chan_t q_alloc_achan(int s,int a,int isize)
{
	int i, j;
	int n = 2 * s * a;

	chan_t chan = (chan_t)calloc(n,sizeof(struct chan));
	for(i = 0; i < n; i ++)
	{
		if(i < s * a){
			chan[i].src = i % s;
			chan[i].dst = i / s;
		}else{
			j = i - s * a;
			chan[i].src = j / s;
			chan[i].dst = j % s;
		}		
		chan[i].id = i;
		chan[i].qTimeout = q_alloc_timeout(isize / s, WHEEL_SIZE);
	}
	return chan;
}

/** 
 * initialize qepoll struct and allocate memory
 * @param[in]	 size  	max size of queue
 * @return		 efd of the event; -1 if the create operation fails
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int 
qepoll_create(qapp_t app, uint32_t size)
{
	int i,nc,qid;
	qepoll_t qe;
	qmag_t qtx;
	//socket_map_t socket;

	if (size <= 0) {
		errno = EINVAL;
		return -1;
 	}

	/*socket = AllocateSocket(app, 1);
	if (!socket) {
		errno = ENFILE;
		return -1;
	}*/

	qtx = g_qepoll[app->app_id];
	qe = qtx->qe;
	if (!qe) {
		return -1;
	}
	
	TRACE_INIT("qepoll structure of size %d created.\n", size);

	//socket->qe = qe;

	/* create event queues and channels*/
	qe->size = size;
	qe->sq_chan = qtx->sq_chan;
	qe->qs_chan = qtx->qs_chan;

	return ++qconf->efd;
}

int 
qepoll_qevent(qmag_t qmag, eque_t event_que, int core, struct qepoll_event *events, int *ev_cnt)
{
	int sockid, app_id;
	int validity = TRUE;
	socket_map_t socket;
 
   	/*TRACE_EVENT("Start wait qevent from event_queue[%d] %p," 
   				"end:%d,start:%d.\n",
   				pri,event_que,event_que->end,event_que->start);*/
   	sockid = event_que->qevent[event_que->start].sockid;
	app_id = event_que->qevent[event_que->start].apid;
   	socket = qconf->g_smap + sockid;
			//qepoll_map->qevents[qepoll_map->qepoll] |= event;
   	if (!(socket->qepoll & 
   			event_que->qevent[event_que->start].events)){
   			TRACE_EXCP("score %d :"
   						"qepoll: %d "
						"start:%d, end:%d "
						"qevent: %p "
#ifdef QVDEB
						"time_add: %llu "
						"flow_state: %d "
#endif
						"dire: %d "
   	 	 				"fd: %d "
   	 	 				"event: %d\n",
						core,
   						socket->qepoll,
						event_que->start, event_que->end,
						&event_que->qevent[event_que->start],
#ifdef QVDEB
						event_que->qevent[event_que->start].time_sp,
						event_que->qevent[event_que->start].flow_st,
#endif
						event_que->qevent[event_que->start].dire,
   	 	 				sockid,
   	 	 				event_que->qevent[event_que->start].events);
   			validity = FALSE;
   			DSTAT_ADD(qstack->qepoll_err, 1);
   	}
   	if (!(event_que->qevent[event_que->start].events)){
   		TRACE_EXCP("qepoll_map->qevents:%d,"
   	 	 				"qevent->fd:%d,"
   	 	 				"qevent->event:%d.\n",
   	 	 				socket->qevents,
   	 	 				sockid,
   	 	 				event_que->qevent[event_que->start].events);
   		validity = FALSE;
   	}
   	if (validity) {
		/*if(!socket->ep_data.ptr)
			TRACE_EXIT("socket:%p, id:%d, data.ptr is NULL!\n", socket, sockid);*/
   		events[(*ev_cnt)++] = event_que->qevent[event_que->start];
   		_mm_sfence();
   		/*if(event_que->qevent[event_que->start].dire == APP_TO_QE)
   			DSTAT_ADD(qstack->wait_appev_num, 1);
   		else
   			DSTAT_ADD(qstack->wait_stev_num, 1);*/
   		reg[sockid].proc = 0;
#if 0 // modified by shenyifan
   		TRACE_EVENT("Wait qevent from qepoll_map:%p, "
   					"events[%d]:%p, "
   					"events type:%d, "
   					"events fd:%d, "
   					"events.data.ptr:%p\n",
   					socket,*ev_cnt-1,
   					events,
   					events[*ev_cnt-1].events,
   					sockid,
   					events[*ev_cnt-1].data.ptr);
#else
		TRACE_CHECKP("@Core %d wait event %p, type:%d, fd:%d\n", 
				core, &event_que->qevent[event_que->start], 
				events[*ev_cnt-1].events, sockid);
#endif
   		//qepoll_map->qevents[qepoll_map->qepoll] 
   		//	&= (~event_que->qevent[event_que->start].events);
   	}
   	event_que->start = (event_que->start + 1) % event_que->size;
	return validity;
}

/** 
 * wait for events in polling/coroutine mode
 * @param[in]	 efd  		max size of queue
 * @param[in]	 events  	qepoll events data structures
 * @param[in]	 maxevents  the maximum number of the events can be fetched
 * @param[in]	 timeout  	the timeout of qepoll wait for events
 * @return		 events number to be processed; -1 means timeout
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int 
qepoll_wait(qapp_t app, struct qepoll_event *events, int maxevents, int timeout)
{  
	qmag_t qmag;
	socket_map_t socket;
	qepoll_t qepoll;
	eque_t  event_que, eq_h;
	qTimer_t apTimeout;
	int num_stack;
	int sockid,validity,dire = 0;
	int i,j,pri,idx,ns,num_events = 0;
	uint32_t ev_cnt = 0;
	
	_mm_sfence();

	num_stack = get_qconf()->num_stack;

	qmag = g_qepoll[app->app_id];
 	if (!qmag) 
	{
		TRACE_EXCP("Qepoll manager is NULL.\n");
		return -1;
	}
		
	if (app->app_id < 0 || app->app_id >= MAX_SERVER_NUM) 
	{
		TRACE_EXCP("Qepoll id %d out of range.\n", app->app_id);
		return -EBADF;
 	}

 	for(ns = 0; ns < num_stack; ns++)
	{
		qepoll = qmag->qe;
		
	 	if (!qepoll || !events || maxevents <= 0) 
		{
			TRACE_EXCP("Qepoll or events buffer or maxevents error.\n");
			return -EINVAL;
	 	}
		
		for(dire = 0; dire < 2; dire++){
			if(dire)
				apTimeout = qepoll->qs_chan[ns].qTimeout;
			else
				apTimeout = qepoll->sq_chan[ns].qTimeout;

			if(timeout > 0) //start of timeout process
			{

				if( GetTickMS() < apTimeout->ullStart )
				{ 
					TRACE_EXCP("Time backdate:allNow:%llu,apTimeout->ullStart:%llu!\n", 
							GetTickMS(), apTimeout->ullStart);
					return 0;
				}	
				
				if( apTimeout->ullStart == 0 )
				{ 
					apTimeout->ullStart = GetTickMS();
					apTimeout->llStartIdx[0] = 0;
					apTimeout->llStartIdx[1] = 0;
				}
				
				int cnt = GetTickMS() - apTimeout->ullStart + 1;
				
				if( cnt > apTimeout->qItemSize[1] )
				{ 
					cnt = apTimeout->qItemSize[1];
				}
			
			
				if( cnt < 0 )
				{ 
					TRACE_EXCP("Wrong events number in Timewheel!\n");
					return 0;
				}


				/* TRACE_EPOLL("apTimeout:%p,"
				 * 			   "allNow:%llu,"
				 * 			   "ullStart:%llu,"
				 * 			   "pri:%d,cnt:%d.\n",
						apTimeout,allNow,apTimeout->ullStart,pri,cnt);*/

				for( i = 0;i < cnt;i++)
				{
					idx = ( apTimeout->llStartIdx[1] + i) % apTimeout->qItemSize[1];
					event_que = apTimeout->qEvque[1] + idx;
					/*TRACE_EPOLL("apTimeout:%p,\npri:%d,idx:%d,\nnum_events:%d.\n",
					 *	       apTimeout,pri,idx,num_events);*/
					while(!(event_que->end == event_que->start) && ev_cnt < maxevents)
					{
						validity = qepoll_qevent(qmag, event_que, app->app_id, events, &ev_cnt);
						if(validity)
							DSTAT_ADD(get_global_ctx()->high_event_queue[app->app_id], 1);
						_mm_sfence();
					}
				} 
				apTimeout->llStartIdx[1] += cnt - 1;

				if( cnt > apTimeout->qItemSize[0] )
				{
					cnt = apTimeout->qItemSize[0];
				} 
				
				for( i = 0;i < cnt;i++)
				{ 
					idx = ( apTimeout->llStartIdx[0] + i) % apTimeout->qItemSize[0];
					event_que = apTimeout->qEvque[0] + idx;
					/*TRACE_EPOLL("apTimeout:%p,\npri:%d,idx:%d,\nnum_events:%d.\n",
					 *	       apTimeout,pri,idx,num_events);*/
					while(!(event_que->end == event_que->start) && ev_cnt < maxevents)
					{
						validity = qepoll_qevent(qmag, event_que, app->app_id, events, &ev_cnt);
						_mm_sfence();
						if(validity)
							DSTAT_ADD(get_global_ctx()->low_event_queue[app->app_id], 1);

						for( j = 0;j < cnt;j++)
						{
							idx = ( apTimeout->llStartIdx[1] + j) % apTimeout->qItemSize[1];
							eq_h = apTimeout->qEvque[1] + idx;
							while(!(eq_h->end == eq_h->start) && ev_cnt < maxevents)
							{
								validity = qepoll_qevent(qmag, eq_h, app->app_id, events, &ev_cnt);
								_mm_sfence();
								if(validity)
									DSTAT_ADD(get_global_ctx()->high_event_queue[app->app_id], 1);
							}
						}
						apTimeout->llStartIdx[1] += cnt - 1;
					}
				}
				apTimeout->ullStart = GetTickMS();
				apTimeout->llStartIdx[0] += cnt - 1;
			}else //start of non-timeout process
			{
				event_que = apTimeout->qEvque[1];
				while((event_que->end != event_que->start) && ev_cnt < maxevents)
				{
					TRACE_EVENT("From Stack %d,apTimeout:%p,\npri:%d,\nnum_events:%d.\n",
							ns, apTimeout,pri,num_events);
					validity = qepoll_qevent(qmag, event_que, app->app_id, events, &ev_cnt);
					if(validity)
						DSTAT_ADD(get_global_ctx()->high_event_queue[app->app_id], 1);
						_mm_sfence();
				}
				event_que = apTimeout->qEvque[0];
				while((event_que->end != event_que->start) && ev_cnt < maxevents)
				{
					if(app->app_id == 0)
						TRACE_EVENT("From Stack %d,"
									 "apTimeout:%p,"
									 "\npri:%d,"
									 "\nnum_events:%d.\n",
									ns, 
									apTimeout,
									pri,
									num_events);
					validity = qepoll_qevent(qmag, event_que, app->app_id, events, &ev_cnt);
					_mm_sfence();
					if(validity)
						DSTAT_ADD(get_global_ctx()->low_event_queue[app->app_id], 1);

					eq_h = apTimeout->qEvque[1];
					while((eq_h->end != eq_h->start) && ev_cnt < maxevents)
					{
						validity = qepoll_qevent(qmag, eq_h, app->app_id, events, &ev_cnt);
						_mm_sfence();
						if(validity)
							DSTAT_ADD(get_global_ctx()->high_event_queue[app->app_id], 1);
					}
				}
			}//end of timeout judge
 		}//end of dire circle
 	}
	if (!ev_cnt) {
		int core_id = get_app_context(app->app_id)->core_id; 
		rtctx_t rt_ctx = get_core_context(core_id)->rt_ctx;
		if (rt_ctx->qstack) {
			struct thread_wakeup *wakeup_ctx = &rt_ctx->wakeup_ctx;
		
			wakeup_ctx->knocked = 0;
			wakeup_ctx->waiting = 0;
			TRACE_THREAD("The application thread fall asleep @ Core %d\n",
					core_id);
			wakeup_ctx->pending = 1;
	#ifdef PTHREAD_SCHEDULING
			pthread_mutex_lock(&wakeup_ctx->epoll_lock);
			pthread_cond_wait(&wakeup_ctx->epoll_cond, &wakeup_ctx->epoll_lock);
			pthread_mutex_unlock(&wakeup_ctx->epoll_lock);
	#endif
	#ifdef COROUTINE_SCHEDULING
//			runtime_schedule(rt_ctx, get_sys_ts());
			yield_to_stack(rt_ctx);
	#endif
			wakeup_ctx->pending = 0;
			TRACE_THREAD("The application thread is waken up @ Core %d\n",
					core_id);
		}
	}
	return ev_cnt;
} 

// add by shenyifan
int 
qepoll_wait_pri(int efd, struct qepoll_event *events_h, 
		struct qepoll_event *events_l, int maxevents, int *high_num, 
		int *low_num, int timeout)
{  
	qmag_t qmag;
	socket_map_t qepoll_map;
	qepoll_t qepoll;
	eque_t  event_que, eq_h;
	qTimer_t apTimeout;
	int num_qepoll;
	int sockid,validity,dire = 0;
	int i,j,pri,idx,ns,num_events = 0;
	uint32_t ev_cnt_h = 0;
	uint32_t ev_cnt_l = 0;
	
	_mm_sfence();

	num_qepoll = get_qconf()->num_server;

 	for(ns = 0; ns < num_qepoll; ns++)
	{
		qmag = g_qepoll[ns];
 	 	if (!qmag) 
		{
			TRACE_EXCP("Qepoll manager is NULL.\n");
			return -1;
		}
		
	 	if (efd < 0 || efd >= MAX_FLOW_NUM) 
		{
			TRACE_EXCP("Qepoll id %d out of range.\n", efd);
			return -EBADF;
 		}

		qepoll = qmag->qe;
		
	 	if (!qepoll || !events_h || !events_l || !high_num || !low_num || 
				maxevents <= 0) 
		{
			TRACE_EXCP("Qepoll or events buffer or maxevents error.\n");
			return -EINVAL;
	 	}
		
		for(dire = 0; dire < 2; dire++){
			if(dire)
				apTimeout = qepoll->qs_chan[efd].qTimeout;
			else
				apTimeout = qepoll->sq_chan[efd].qTimeout;

			if(timeout > 0){

				if( GetTickMS() < apTimeout->ullStart )
				{ 
					TRACE_EXCP("Time backdate:allNow:%llu,apTimeout->ullStart:%llu!\n", 
							GetTickMS(), apTimeout->ullStart);
					return 0;
				}	
				
				if( apTimeout->ullStart == 0 )
				{ 
					apTimeout->ullStart = GetTickMS();
					apTimeout->llStartIdx[0] = 0;
					apTimeout->llStartIdx[1] = 0;
				}
				
				int cnt = GetTickMS() - apTimeout->ullStart + 1;
				
				if( cnt > apTimeout->qItemSize[1] )
				{ 
					cnt = apTimeout->qItemSize[1];
				}
			
			
				if( cnt < 0 )
				{ 
					TRACE_EXCP("Wrong events number in Timewheel!\n");
					return 0;
				}


				/* TRACE_EPOLL("apTimeout:%p,"
				 * 			   "allNow:%llu,"
				 * 			   "ullStart:%llu,"
				 * 			   "pri:%d,cnt:%d.\n",
						apTimeout,allNow,apTimeout->ullStart,pri,cnt);*/

				for( i = 0;i < cnt;i++)
				{
					idx = ( apTimeout->llStartIdx[1] + i) % apTimeout->qItemSize[1];
					event_que = apTimeout->qEvque[1] + idx;
					/*TRACE_EPOLL("apTimeout:%p,\npri:%d,idx:%d,\nnum_events:%d.\n",
					 *	       apTimeout,pri,idx,num_events);*/
					while(!(event_que->end == event_que->start) && 
							ev_cnt_h+ev_cnt_l < maxevents)
					{
						validity = qepoll_qevent(qmag, event_que, ns, events_h, 
								&ev_cnt_h);
						if(validity)
							DSTAT_ADD(get_global_ctx()->high_event_queue[ns], 1);
						_mm_sfence();
					}
				} 
				apTimeout->llStartIdx[1] += cnt - 1;

				if( cnt > apTimeout->qItemSize[0] )
				{
					cnt = apTimeout->qItemSize[0];
				} 
				
				for( i = 0;i < cnt;i++)
				{ 
					idx = ( apTimeout->llStartIdx[0] + i) % apTimeout->qItemSize[0];
					event_que = apTimeout->qEvque[0] + idx;
					/*TRACE_EPOLL("apTimeout:%p,\npri:%d,idx:%d,\nnum_events:%d.\n",
					 *	       apTimeout,pri,idx,num_events);*/
					while(!(event_que->end == event_que->start) && 
							ev_cnt_h + ev_cnt_l < maxevents)
					{
						validity = qepoll_qevent(qmag, event_que, ns, events_l, 
								&ev_cnt_l);
						_mm_sfence();
						if(validity)
							DSTAT_ADD(get_global_ctx()->low_event_queue[ns], 1);

						for( j = 0;j < cnt;j++)
						{
							idx = ( apTimeout->llStartIdx[1] + j) % apTimeout->qItemSize[1];
							eq_h = apTimeout->qEvque[1] + idx;
							while(!(eq_h->end == eq_h->start) && 
									ev_cnt_h + ev_cnt_l < maxevents)
							{
								validity = qepoll_qevent(qmag, eq_h, ns, events_h, 
										&ev_cnt_h);
								_mm_sfence();
								if(validity)
									DSTAT_ADD(get_global_ctx()->high_event_queue[ns], 1);
							}
						}
						apTimeout->llStartIdx[1] += cnt - 1;
					}
				}
				apTimeout->ullStart = GetTickMS();
				apTimeout->llStartIdx[0] += cnt - 1;
			}else{
				event_que = apTimeout->qEvque[1];
				while((event_que->end != event_que->start) && 
						ev_cnt_h + ev_cnt_l < maxevents)
				{
					TRACE_EVENT("From Qepoll %d,apTimeout:%p,\npri:%d,\nnum_events:%d.\n",
							ns, apTimeout,pri,num_events);
					validity = qepoll_qevent(qmag, event_que, ns, events_h, 
							&ev_cnt_h);
					if(validity)
						DSTAT_ADD(get_global_ctx()->high_event_queue[ns], 1);
					_mm_sfence();
				}
				event_que = apTimeout->qEvque[0];
				/*TRACE_EPOLL("apTimeout:%p,\npri:%d,idx:%d,\nnum_events:%d.\n",
				 *	       apTimeout,pri,idx,num_events);*/
				while((event_que->end != event_que->start) && 
						ev_cnt_h + ev_cnt_l < maxevents)
				{
					validity = qepoll_qevent(qmag, event_que, ns, events_l, 
							&ev_cnt_l);
					_mm_sfence();
					if(validity)
						DSTAT_ADD(get_global_ctx()->low_event_queue[ns], 1);

					eq_h = apTimeout->qEvque[1];
					while((eq_h->end != eq_h->start) && 
							ev_cnt_h + ev_cnt_l < maxevents)
					{
						validity = qepoll_qevent(qmag, eq_h, ns, events_h, 
								&ev_cnt_h);
						_mm_sfence();
						if(validity)
							DSTAT_ADD(get_global_ctx()->high_event_queue[ns], 1);
					}
				}
			}
 		}
 	}
	return ev_cnt_h + ev_cnt_l;
} 

void qepoll_loop(qepoll_t qtx,pfn_coroutine_t pfn,void *arg)
{ 
	struct qepoll_event *result = qtx->result;
	int i;
	for(;;)
 	{
		int ret = qepoll_wait( qtx->qfd,result,qtx->size, 1 );
		
		unsigned long long now = GetTickMS();
		for(i=0;i<ret;i++)
		{
			qTimewheelItem_t *item = (qTimewheelItem_t*)result[i].data.ptr;
			item->bTimeout = true;
			if(item){
				if( item->pfnPrepare )
				{
					item->pfnPrepare(item,result+i);
 				}
				if(item->bTimeout && now < item->ullExpireTime)
 				{
					int ret = AddTimeEvent(result[i].events, now, result + i);
					if(!ret)
 					{
						item->bTimeout = false;
						continue;
					}
				}
				if(item->pfnProcess)
 				{
					item->pfnProcess(item);
					//qepoll_ctl(qtx->qfd,Q_EPOLL_CTL_MOD,result[i].fd,result + i);
				}
 			}
 		}

		if( pfn )
		{
			if( -1 == pfn( arg ) )
			{
				break;
			}
		}

	}
}

struct qepoll_event*
CreateQevent(uint8_t pri, uint8_t core, int sockid, int dst, uint8_t type)
{
	int qid;
	struct qepoll_event *qevent;
	
	if(core >= qconf->num_stack){
		qid = core - qconf->num_stack;
		if(g_qarg[qid].app_count >= (QEPOLL_SIZE / qconf->num_app))
			g_qarg[qid].app_count = 0;
		qevent = g_qarg[qid].app_buff + g_qarg[qid].app_count;
		qevent->dire = APP_TO_QE;
		//qevent->data.ptr = g_qarg[qid].aTimeItems_buff + g_qarg[qid].app_count;	
		g_qarg[qid].app_count++;
	}else{
		qid = core;
		if(g_sarg[qid].qev_count >= (QEPOLL_SIZE / qconf->num_stack))
			g_sarg[qid].qev_count = 0;
		qevent = g_sarg[qid].qevent_buff + g_sarg[qid].qev_count;
		qevent->dire = STACK_TO_QE;
		//qevent->data.ptr = g_qarg[qid].qTimeItems_buff + g_qarg[qid].qev_count;	
		g_sarg[qid].qev_count++;
	}	
	
	qevent->pri  = pri;
	qevent->apid = dst;
	qevent->core = core;
	qevent->nature = type;
	qevent->sockid = sockid;
	qevent->timeout = -1;
			
	return qevent;
}

int 
q_distribute_event(struct qepoll_event *qevent, bool proc)
{
	int src, dst, sockid;

	src = qevent->core;
	sockid = qevent->sockid;
	
	if(qevent->nature != Q_ACCEPT && proc)
	{
		if(reg[sockid].pr_core > -1)
			dst = reg[sockid].pr_core;
		else 
			TRACE_EXIT("Socket %d has event from %d\n" 
					"nature:%d, proc state:%d, pr_core:%d!\n", 
					qevent->sockid, src, qevent->nature, 
					proc, reg[sockid].pr_core);
			 
	}else{
		dst = qevent->apid;//qevent->sockid % num_app;
		if(unlikely(dst < 0))
			dst = qevent->sockid % qconf->num_app;
		reg[sockid].proc = 1;
		reg[sockid].pr_core = dst;
	}
	
//	DSTAT_ADD(get_global_ctx()->event_add[src][dst], 1);
	return dst;	
}

int
AddTimeEvent(uint32_t event,uint64_t allNow, struct qepoll_event *qevent)
{
	int 	  	core, qid;
	socket_map_t socket;
	
	bool        proc;
	int 		chan_id;
	chan_t 	    chan;

	int 		idx, sockid, ev_index, pri;
	eque_t    	event_queue;
	
	uint64_t 			diff;	
	qTimer_t  		  	apTimeout;
	qTimewheelItem_t 	*apItem;

	//single_ts_start();

	pri    = qevent->pri;
	core   = qevent->core;
	sockid = qevent->sockid;
	
	int num_stack = qconf->num_stack;
		
	if (get_global_ctx()->socket_table->sockets[sockid].socktype 
			!=SOCK_TYPE_LISTENER) {
		tcp_stream_t cur_stream = 
				get_global_ctx()->socket_table->sockets[sockid].stream;
		if (cur_stream->state >=TCP_ST_LAST_ACK || 
				cur_stream->state == TCP_ST_CLOSED) {
			TRACE_EXCP("try to raise event to a stream "
					"with wrong state %d @ Stream %d @ Core %d\n", 
					cur_stream->state, cur_stream->id, core);
		}
	}
		
	if(qevent->dire == STACK_TO_QE){
		DSTAT_ADD(qstack->stack_event_num, 1);
		proc = reg[sockid].proc;
		qid  = q_distribute_event(qevent, proc);
		chan = g_qepoll[qid]->qe->sq_chan;
		chan_id = core;
		DSTAT_ADD(get_global_ctx()->stack_contexts[core]->event_to_add, 1);
		if(chan[chan_id].src == core 
			&& chan[chan_id].dst == qid
			&& chan[chan_id].id == (chan_id + qid * num_stack)){
			apTimeout = chan[chan_id].qTimeout;
		}else{
			TRACE_EXIT("Socket %d has type %d event from %d\n" 
					"chan_id[%d] chan.src:%d, chan.dst:%d\n"
				    "error event src:%d, event dst %d\n" 
					"in Distribute event!\n", 
					sockid, qevent->dire,qevent->nature,
					chan[chan_id].id ,chan[chan_id].src, 
					chan[chan_id].dst,
					core, chan_id);
		}
	}else{
		DSTAT_ADD(qstack->app_event_num, 1);
		qid  = qevent->apid;
		chan = g_qepoll[qid]->qe->qs_chan;
		chan_id = sockid % num_stack;
		if(chan_id < 0){
			TRACE_EXCP("CO_ERR: AddTimeout from app line %d"
					   "chan_id %d event src core %d\n",
					__LINE__,chan_id,core);

			return __LINE__;
 		}
		apTimeout = chan[chan_id].qTimeout;
 	}
	

	if(qevent->timeout > 0)
	{
		if(g_qarg[qid].app_count >= (QEPOLL_SIZE / qconf->num_app))
             g_qarg[qid].app_count = 0;
		apItem = g_qarg[qid].aTimeItems_buff + g_qarg[qid].app_count;
		
		g_qarg[qid].app_count++;

		if( apTimeout->ullStart == 0 )
		{
			apTimeout->ullStart = allNow;
			apTimeout->llStartIdx[pri] = 0;
		}
		if( allNow < apTimeout->ullStart )
		{
			/*TRACE_EXCP("CO_ERR: AddTimeout line %d allNow %llu apTimeout->ullStart %llu\n",
						__LINE__,allNow,apTimeout->ullStart);

			return __LINE__;*/
			diff = 0;
		}else{
			/*if( apItem->ullExpireTime < allNow )
			{
				TRACE_EXCP("CO_ERR: AddTimeout line %d" 
							   "apItem->ullExpireTime %llu" 
							   "allNow %llu "
							   "apTimeout->ullStart %llu\n",
							__LINE__,apItem->ullExpireTime,allNow,apTimeout->ullStart);

				return __LINE__;
			}*/
			apItem->ullExpireTime = allNow + qevent->timeout;
			diff = apItem->ullExpireTime - apTimeout->ullStart;
			if(apTimeout->ullStart > apItem->ullExpireTime){
				diff = 0;
			}else if( diff >= apTimeout->iItemSize ){
				diff = apTimeout->iItemSize - 1;
				TRACE_EXCP("CO_ERR: AddTimeout line %d " 
								"apItem->ullExpireTime %llu " 
								"apTimeout->ullStart %llu " 
								"diff %d\n",
							__LINE__,apItem->ullExpireTime,apTimeout->ullStart,diff);

				//return __LINE__;
			}
		}
		idx = ( apTimeout->llStartIdx[pri] + diff ) % apTimeout->qItemSize[pri];
		event_queue = apTimeout->qEvque[pri] + idx;
	}else{
		event_queue = apTimeout->qEvque[pri];
	}
	
	if(likely(event_queue)){ 
		socket = qconf->g_smap + sockid;	
		socket->qepoll = event;
		socket->qevents |= event;
		event_queue->qevent[event_queue->end].pri = pri;
		ev_index = event_queue->end;
		event_queue->qevent[ev_index].sockid = qevent->sockid;
		event_queue->qevent[ev_index].events = event;
		event_queue->qevent[ev_index].data = socket->ep_data;
#ifdef QVDEB
		/****Just for Debug****/
		event_queue->qevent[ev_index].dire = qevent->dire;
		event_queue->qevent[ev_index].apid = qevent->apid;
		event_queue->qevent[ev_index].core = qevent->core;
		event_queue->qevent[ev_index].time_sp = get_time_ns();
		event_queue->qevent[ev_index].flow_st = qevent->flow_st;
#endif
		
		TRACE_CHECKP("add event %p dire:%d sockid:%d "
#ifdef QVDEB
				"cur_ts:%llu"
#endif				
				"\n",
				&event_queue->qevent[ev_index], qevent->dire, qevent->sockid
#ifdef QVDEB
				, event_queue->qevent[ev_index].time_sp
#endif				
				);
		if(unlikely(EventQueueFull(event_queue))) {
			TRACE_EXCP("Event Queue %p is Full!\n",event_queue);
			DSTAT_ADD(qstack->queue_full_num,1);
			return -1;
		}else{
			_mm_sfence();
			event_queue->end = (event_queue->end + 1) % event_queue->size;
			DSTAT_ADD(get_global_ctx()->event_add_num[core][qid], 1);
		}
	}else{
		TRACE_EXCP("Event Queue is Null!\n");
		return -1;
	}
	
//	TRACE_EPOLL("Add QEPOLL event socket %d into" 
//			    " event_queue[%d] %p, idx:%d, "
//				"ullStart:%llu, "
//				"end:%d, start:%d.\n",
//				qevent->sockid,pri,event_queue,
//				idx,apTimeout->ullStart,
//				event_queue->end,event_queue->start
//	);
	
	//single_ts_end();

	return 0;
} 

/** 
 * for coroutine to get qepoll fd and structure
 * @return		 qepoll structure
 * @ref 		 qepoll.h
 * @see
 * @note
 */
qepoll_t q_alloc_qepoll()
{
	qepoll_t qe = g_qepoll[qconf->efd]->qe;

	qconf->efd++;

	return qe;
}

/******************************************************************************/
/** 
 * initialize qepoll manager handler for allocating fd and map list
 * @return		 qepoll manager handler
 * @ref 		 qepoll.h
 * @see
 * @note
 */
void
q_init_manager(int stack, int server)
{ 
	int i,j;
	int num_stack,num_server;
	qmag_t qmag;
	qepoll_t qe;

	qconf_init(stack, server, 1);

	qconf = get_qconf();
	
	qstack = get_global_ctx()->stack_contexts[0];
//	qstack = (qstack_t)calloc(1,sizeof(struct qstack_context));

	num_server = qconf->num_server;
	num_stack = qconf->num_stack;
	
	g_qepoll = (qmag_t *)calloc(num_server,sizeof(qmag_t));
	g_qarg = (qarg_t)calloc(num_server,sizeof(struct qarg));
	g_sarg = (qarg_t)calloc(num_stack,sizeof(struct qarg));
	qconf->g_smap = (socket_map_t)calloc(MAX_FLOW_NUM, sizeof(struct socket_map));
	if (!qconf->g_smap) {
		perror("Failed to allocate memory for stream map.\n");
		return NULL;
	}
	//TAILQ_INIT(&qconf->free_smap);
	for (j = 0; j < MAX_FLOW_NUM; j++) {
		qconf->g_smap[j].id = j;
		//qmag->qmap[j].qevents = (uint32_t*)calloc(5, sizeof(uint32_t));
		if(AllocateSocket(qconf->g_smap + j, 1))
			perror("Failed to init values for socket map.\n");
		//TAILQ_INSERT_TAIL(&qconf->free_smap, &qconf->g_smap[j], free_smap_link);
	}

	chan_t chan = q_alloc_achan(num_stack,num_server,QEPOLL_SIZE / num_server);
	
	for(i = 0; i < num_stack; i++){
		g_sarg[i].qev_count = 0;
		g_sarg[i].manager_buff = (qmag_t)calloc(1, sizeof(struct qepoll_manager));
		g_sarg[i].qevent_buff = (struct qepoll_event*)calloc(QEPOLL_SIZE / num_stack, 
									sizeof(struct qepoll_event));
		g_sarg[i].app_buff = g_qarg[i].qevent_buff + QEPOLL_SIZE / (2 * num_stack);
	}
	for(i = 0; i < num_server; i++){
		g_qarg[i].qev_count = 0;
		g_qarg[i].manager_buff = (qmag_t)calloc(1, sizeof(struct qepoll_manager));
		g_qarg[i].qevent_buff = (struct qepoll_event*)calloc(QEPOLL_SIZE / num_server, 
									sizeof(struct qepoll_event));
		g_qarg[i].app_buff = g_qarg[i].qevent_buff + QEPOLL_SIZE / (2 * num_server);
		g_qarg[i].qTimeItems_buff = (qTimewheelItem_t *)calloc(MAX_FLOW_NUM / num_server,
									sizeof(qTimewheelItem_t));
		g_qarg[i].aTimeItems_buff = g_qarg[i].qTimeItems_buff + MAX_FLOW_NUM / (2 * num_server);

		qmag = g_qarg[i].manager_buff;
		g_qepoll[i] = qmag;
		qe = (qepoll_t*)calloc(1, sizeof(qepoll_t));
		qe->qfd = i;
		qmag->qe = qe;	
		qmag->sq_chan = chan + num_stack * i;
		qmag->qs_chan = chan + num_stack * (num_server + i);
	}

	for (j = 0; j < MAX_FLOW_NUM; j++) {
		reg[j].proc = 0;
		reg[j].pr_core = -1;
	}
	
//#endif
}

void 
qconf_init(int stack, int server, int app)
{
	qconf = (qconfig_t)calloc(1, sizeof(struct qconfig_epoll));
	qconf->num_server  = server;
	qconf->num_stack  = stack;
	qconf->num_app = app;
	qconf->efd = -1;
}

chan_t
qepoll_get_thread_chan(int stk_id, int srv_id)
{	
	qmag_t qmag;
	qmag = g_qepoll[srv_id];
	return (qmag->sq_chan + srv_id);
}

inline struct qevent_reg
get_ev_stat(int src, int sockid)
{
	return reg[sockid];
}

inline qconfig_t
get_qconf()
{
	return qconf;
}

void*
qstack_get()
{
	return (void*)qstack;
}
/******************************************************************************/
// tools for user explicitly get priority-based events
inline int 
qepoll_mgt_wait(int app_id, struct qepoll_event *events, int maxevents, int pri)
{  
	qmag_t qmag;
	socket_map_t qepoll_map;
	qepoll_t qepoll;
	eque_t  event_que, eq_h;
	qTimer_t apTimeout;
	int num_stack;
	int sockid,validity,dire = 0;
	int i,j,idx,ns,num_events = 0;
	uint32_t ev_cnt = 0;
	
	_mm_sfence();

	num_stack = get_qconf()->num_stack;

	qmag = g_qepoll[app_id];

        if (!qmag)
        {
                TRACE_EXCP("Qepoll manager is NULL.\n");
                return -1;
        }

 	if (!events || maxevents <= 0) 
	{
		TRACE_EXCP("missing events buffer or maxevents.\n");
		return -EINVAL;
 	}

	if (app_id < 0 || app_id >= MAX_SERVER_NUM)
    {
        TRACE_EXCP("Qepoll id %d out of range.\n", app_id);
        return -EBADF;
    }

 	for(ns = 0; ns < num_stack; ns++)
	{	
		qepoll = qmag->qe;
	 	if (!qepoll || !events || maxevents <= 0) 
		{
			TRACE_EXCP("missing Qepoll %d.\n", ns);
			return -EINVAL;
	 	}
		
		apTimeout = qepoll->sq_chan[ns].qTimeout;

		event_que = apTimeout->qEvque[pri];
		while((event_que->end != event_que->start) && ev_cnt < maxevents)
		{
			TRACE_EVENT("From Qepoll %d,apTimeout:%p,\npri:%d,\nnum_events:%d.\n",
					ns, apTimeout,pri,num_events);
			validity = qepoll_qevent(qmag, event_que, ns, events, &ev_cnt);
			if (validity) {
				if (pri) {
					DSTAT_ADD(get_global_ctx()->high_event_queue[app_id], 1);
				} else {
					DSTAT_ADD(get_global_ctx()->low_event_queue[app_id], 1);
				}
			}
			_mm_sfence();
		}
 	}
	return ev_cnt;
} 

inline void
evmgtq_init(evmgtq_t q, int size)
{
	q->events = q_alloc_epres(size);
	q->num = 0;
	q->p = 0;
}
/*----------------------------------------------------------------------------*/
void 
eventmgt_init(evmgt_t evmgt, int maxsize, int lb)
{
	evmgt->size = maxsize;
	evmgt->lb = lb; 
	evmgtq_init(&evmgt->events_h, maxsize);
	evmgtq_init(&evmgt->events_l, lb);
} 

inline struct qepoll_event*
evmgtq_get(evmgtq_t q)
{
	if (q->num && (q->p < q->num)) {
		return &q->events[q->p++];
	} else {
		return NULL;
	}
}

// fetch events from epoll
inline int
evmgtq_fetch_events(evmgtq_t q, int app_id, int maxevents, int pri)
{
	q->p = 0;
	q->num = qepoll_mgt_wait(app_id, q->events, maxevents, pri);
	return q->num;
}

struct qepoll_event*
eventmgt_get(int efd, struct event_mgt *mgt)
{
	struct qepoll_event *ret=NULL;
	int core_id = get_app_context(efd)->core_id; 
	rtctx_t rt_ctx = get_core_context(core_id)->rt_ctx;
	struct thread_wakeup *wakeup_ctx = &rt_ctx->wakeup_ctx;
	systs_t	cur_ts_us = get_time_us();
	rt_ctx->last_event_check_ts = cur_ts_us;
	rt_check(rt_ctx, cur_ts_us);

	if (ret=evmgtq_get(&mgt->events_h)) {
		return ret;
	} else {
		// empty high_event queue, fetch and try again
		evmgtq_fetch_events(&mgt->events_h, efd, mgt->size, 1);
		if (ret=evmgtq_get(&mgt->events_h)) {
			return ret;
		}
	}

	if (ret=evmgtq_get(&mgt->events_l)) {
		return ret;
	} else {
		// empty high_event queue, fetch and try again
		evmgtq_fetch_events(&mgt->events_l, efd, mgt->lb, 0);
		ret=evmgtq_get(&mgt->events_l);
	}

	if (ret == NULL && rt_ctx->qstack != NULL) {
		wakeup_ctx->knocked = 0;
		wakeup_ctx->waiting = 0;
		TRACE_THREAD("The application thread fall asleep @ Core %d\n",
				core_id);
		wakeup_ctx->pending = 1;
	#ifdef PTHREAD_SCHEDULING
		pthread_mutex_lock(&wakeup_ctx->epoll_lock);
		pthread_cond_wait(&wakeup_ctx->epoll_cond, &wakeup_ctx->epoll_lock);
		pthread_mutex_unlock(&wakeup_ctx->epoll_lock);
	#endif
	#ifdef COROUTINE_SCHEDULING
//		runtime_schedule(rt_ctx, get_sys_ts());
		yield_to_stack(rt_ctx);
	#endif
		wakeup_ctx->pending = 0;
		TRACE_THREAD("The application thread is waken up @ Core %d\n",
				core_id);
	}

	return ret;
}
/******************************************************************************/
