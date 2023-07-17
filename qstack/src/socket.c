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
 * @file socket.c
 * @brief socket processing
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.7.22
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.7.22
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date: 2019.1.23
 *   	Author: Shen Yifan
 *   	Modification: change socket_alloc into multicore mode
 *   3. Date: 
 *   	Author:
 *   	Modification:
 */
#include "socket.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
/******************************************************************************/
/* functions */
socket_table_t 
socket_table_init()
{
	int i = 0;
	int core;
	// socket can be alloced for every stack thread
	int chunk_num = MAX_SOCKET_PSTACK; 
	
	if (CONFIG.max_concurrency > MAX_SOCKET_NUM) {
		TRACE_EXIT("too big max_concurrency or too small MAX_SOCKET_NUM!\n");
	}
	
	socket_table_t table = (socket_table_t)calloc(1, 
			sizeof(struct socket_table));
	//TODO: put in huge page
	table->sockets = (socket_t)calloc(MAX_SOCKET_NUM+PUB_SOCKET_NUM, 
			sizeof(struct socket));
	for (core=0; core<CONFIG.stack_thread; core++) { // init stack socket pools
		TAILQ_INIT(&table->free_socket[core]);
		table->free_num[core] = 0;
		for (; i<(core+1)*chunk_num; i++) {
			table->sockets[i].id = i;
			table->sockets[i].qe = NULL;
			table->sockets[i].socktype = SOCK_TYPE_UNUSED;
			memset(&table->sockets[i].saddr, 0, sizeof(struct sockaddr_in));
			table->sockets[i].stream = NULL;
			TAILQ_INSERT_TAIL(&table->free_socket[core], &table->sockets[i], 
					free_link);
			table->free_num[core]++;
		}
	}
	// init public socket pool
	TAILQ_INIT(&table->free_socket_public);
	table->free_public_num = 0;
	for (i=MAX_SOCKET_NUM; i<MAX_SOCKET_NUM+PUB_SOCKET_NUM; i++) {
		table->sockets[i].id = i;
		table->sockets[i].qe = NULL;
		table->sockets[i].socktype = SOCK_TYPE_UNUSED;
		memset(&table->sockets[i].saddr, 0, sizeof(struct sockaddr_in));
		table->sockets[i].stream = NULL;
		TAILQ_INSERT_TAIL(&table->free_socket_public, &table->sockets[i], 
				free_link);
		table->free_public_num++;
	}
	return table;
}
	
socket_t 
socket_alloc(int core_id, int socktype)
{
	socket_table_t table = get_global_ctx()->socket_table;
	socket_t socket;
	if (core_id >= CONFIG.stack_thread) {
		socket = TAILQ_FIRST(&table->free_socket_public);	
		if (socket) {
			TAILQ_REMOVE(&table->free_socket_public, socket, free_link);
			table->free_public_num--;
		} else {
			TRACE_EXCP("failed to alloc socket ifrom public pool!\n");
			return NULL;
		}
	} else {
		socket = TAILQ_FIRST(&table->free_socket[core_id]);	
		if (socket) {
			TAILQ_REMOVE(&table->free_socket[core_id], socket, free_link);
			table->free_num[core_id]--;
		} else {
			TRACE_EXCP("failed to alloc socket @ Core %d!\n", core_id);
			return NULL;
		}
	}

	socket->socktype = socktype;
	socket->opts = 0;
	socket->epoll = 0;
	socket->qe = NULL;
	socket->stream = NULL;
	memset(&socket->saddr, 0, sizeof(struct sockaddr_in));
	memset(&socket->daddr, 0, sizeof(struct sockaddr_in));
//	memset(&socket->ep_data, 0, sizeof(qepoll_data_t));

	return socket;
}

void 
socket_free(int core_id, int sockid)
{
	socket_table_t table = get_global_ctx()->socket_table;
	socket_t socket = &table->sockets[sockid];

	if (socket->socktype == SOCK_TYPE_UNUSED) {
		TRACE_ERR("The socket to be freed should not be UNUSED! "
				        "@ Socket %d socktype %d\n",
						sockid, socket->socktype);
		return;
	}
	
	socket->socktype = SOCK_TYPE_UNUSED;
	socket->epoll = Q_EPOLLNONE;
	if (socket->qe) {
		qepoll_reset(socket->qe->qfd, sockid);
	}
	socket->qe = NULL;

	/* insert into free socket map */
	socket->stream = NULL;
	TAILQ_INSERT_TAIL(&table->free_socket[core_id], socket, free_link);
	table->free_num[core_id]++;
}

struct tcp_listener*
listeners_search(listeners_t listeners, uint16_t port)
{
	int i;
	for (i=0; i<listeners->num; i++) {
		if (listeners->table[i]->socket->saddr.sin_port == port) {
			return listeners->table[i];
		}
	}
	return NULL;
}

int
listeners_insert(listeners_t listeners, struct tcp_listener *listener)
{
	if (listeners->num < MAX_SOCKET_NUM) {
		listeners->table[listeners->num++] = listener;
		return SUCCESS;
	} else {
		return FAILED;
	}
}

int 
listeners_remove(listeners_t listeners, struct tcp_listener *listener)
{
	TRACE_TODO();
}
/******************************************************************************/
int
get_stackid_from_sockid(uint32_t sockid)
{
	return get_global_ctx()->socket_table->sockets[sockid].stream->qstack->stack_id;
}
/*----------------------------------------------------------------------------*/
static inline socket_t
get_socket(int sockid)
{
	return &get_global_ctx()->socket_table->sockets[sockid];
}

qepoll_t
get_qe_from_socket(int sockid)
{
	return get_socket(sockid)->qe;
}

int
bind_socket_with_qe(int sockid, qepoll_t qe)
{
	TRACE_EPOLL("try to bind socket %d to epoll %p\n", sockid, qe);
	socket_t sock = get_socket(sockid);
	if (!sock || sock->qe || !qe) {
		return FALSE;
	} else {
		sock->qe = qe;
		return SUCCESS;
	}
}

uint32_t *
get_epoll_from_socket(int sockid)
{
	return &get_socket(sockid)->epoll;
}

struct qepoll_event *
get_pending_event(int sockid)
{
	socket_t socket = get_socket(sockid);
	TRACE_EPOLL("try to get pending event @ Socket %d\n", sockid);
	if (!socket->stream) {
		return NULL;
	}
	if (socket->socktype == SOCK_TYPE_STREAM && 
			socket->stream->pdev_st == pdev_st_WRITEN) {
		
		socket->stream->pdev_st = pdev_st_READING;
		struct qepoll_event *ret = 
			event_list_pop(&socket->stream->pending_events);
		socket->stream->pdev_st = pdev_st_READ;
		if (ret) {
			TRACE_EVENT("get pending event %p @ Stream %d\n", 
					ret, sockid);
		} else {
			TRACE_EVENT("there is no pending event @ Socket %d\n", sockid);
		}
		return ret;
	} else {
		TRACE_EPOLL("failed to get pending event @ socket %d, "
				"socktype:%d, pdev_st:%d\n", 
				sockid, socket->socktype, socket->stream->pdev_st);
		return NULL;
	}
}

uint32_t inactive_num = 0;
void
socket_scan()
{
#if SCAN_INACTIVE_SOCKET
	int i;
	uint32_t active_num = 0;
	tcp_stream_t cur_stream;
	rcv_buff_t buff;
	mbuf_t mbuf;
	//systs_t cur_ts = get_sys_ts();
	systs_t cur_ts = get_time_ms();
	socket_t socket;
	for (i=0; i<MAX_FLOW_NUM; i++) {
		socket = &get_global_ctx()->socket_table->sockets[i];
		if (socket->socktype != SOCK_TYPE_STREAM) {
			continue;
		}
		cur_stream = socket->stream;
		if (!cur_stream || cur_stream->state != TCP_ST_ESTABLISHED) {
			continue;
		}
		if ((int32_t)(cur_ts - cur_stream->last_active_ts) > INACTIVE_THRESH) {
			TRACE_EXCP("inactive @ Stream %d, port %d, last_ts:%lu\n", 
					cur_stream->id, ntohs(cur_stream->dport), 
					cur_stream->last_active_ts);
			raise_read_event(get_global_ctx()->stack_contexts[0], cur_stream, 
					cur_ts, 0);
			if (inactive_num++>INACTIVE_EXIT_THRESH) {
				TRACE_EXIT("too much inactive flows!\n");
			}
		}
		if ((int32_t)(cur_ts - cur_stream->last_active_ts) < 100000) {
			active_num++;
		}
	}
	TRACE_SCREEN("active TCP stream:\t%u\n====================\n", active_num);
#endif
}
