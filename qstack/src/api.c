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
 * @file api.c
 * @brief APIs for applications
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.6
 * @version 0.1
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.9.6 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */
#ifndef TRACE_LEVEL
//	#define TRACE_LEVEL	TRACELV_DEBUG
#endif
#define N21Q_DEQUEUE_STRONG
/*----------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <sched.h>

#include "tcp_out.h"
#include <sys/ioctl.h>
#include <rte_thash.h>
#include "api.h"
/******************************************************************************/
/* local macros */
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */
static inline qstack_t
get_qstack(qapp_t app)
{
	return get_global_ctx()->stack_contexts[0];
}

static int
close_stream_socket(qapp_t app, int sockid)
{
	tcp_stream_t cur_stream; 
	qstack_t qstack;
	
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	if (!cur_stream) {
		TRACE_EXCP("Socket %d: stream does not exist.\n", sockid);
		errno = ENOTCONN;
		return -1;
	}
	qstack = cur_stream->qstack;

	if (cur_stream->closed) {
		TRACE_EXCP("Socket %d (Stream %u): already closed stream\n", 
				sockid, cur_stream->id);
		return 0;
	}
	cur_stream->closed = TRUE;
	// the socket should be freed at stack thread
//	cur_stream->socket->socktype = SOCK_TYPE_UNUSED;
//	cur_stream->socket = NULL;

	if (cur_stream->state != TCP_ST_ESTABLISHED && 
			cur_stream->state != TCP_ST_CLOSE_WAIT) {
		TRACE_EXCP("Stream %d closed at unexcepted state %d\n", 
				cur_stream->id, cur_stream->state);
		errno = EBADF;
		return -1;
	}
	TRACE_CLOSE("close call is enqueued @ Stream %d @ Stack %d\n", 
			cur_stream->id, qstack->stack_id);
	streamq_enqueue(&qstack->close_queue, app->core_id, cur_stream);
	cur_stream->sndvar.on_closeq = TRUE;
	
	return 0;
}

static uint8_t default_rss_key[] = {
		0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
		0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
		0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
		0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
		0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa,
};
static uint8_t rss_key_be[RTE_DIM(default_rss_key)];
/******************************************************************************/
/* functions */
uint32_t  
q_socket(qapp_t app, int domain, int type, int protocol)
{
	socket_table_t table = get_global_ctx()->socket_table;
	socket_t socket = NULL; 
	if (domain != AF_INET) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (type == SOCK_TYPE_STREAM) {
		type = (int)SOCK_TYPE_STREAM;
	} else {
		errno = EINVAL;
		return -1;
	}
	//TODO: the socket alloc and free may not be multi-core safe!
	socket = socket_alloc(get_sock_pool_map(app), SOCK_TYPE_STREAM);
	if (!socket) {
		errno = ENFILE;
		TRACE_EXCP("failed to allocate socket!\n");
		return -1;
	}
	socket->default_app = app->app_id;

	return socket->id;
}

int 
q_bind(qapp_t app, int sockid, const struct sockaddr *addr, socklen_t addrlen)
{
	socket_table_t table = get_global_ctx()->socket_table;
	socket_t socket;

	TRACE_CHECKP("q_bind() was called @ Socket %d\n", sockid);
	// note that the max_concurrency may be 0 at test mode
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	socket = &table->sockets[sockid];
	if (socket->socktype == SOCK_TYPE_UNUSED) {
		TRACE_EXCP("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}
	
	if (socket->socktype != SOCK_TYPE_STREAM && 
			socket->socktype != SOCK_TYPE_LISTENER) {
		TRACE_EXCP("Not a stream socket id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (!addr) {
		TRACE_EXCP("Socket %d: empty address!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	if (socket->opts & SOCK_OPT_ADDR_BIND) {
		TRACE_EXCP("Socket %d: adress already bind for this socket.\n", sockid);
		errno = EINVAL;
		return -1;
	}

	/* we only allow bind() for AF_INET address */
	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		TRACE_EXCP("Socket %d: invalid argument!\n", sockid);
		errno = EINVAL;
		return -1;
	}

	/* TODO: validate whether the address is already being used */
	socket->saddr = *(struct sockaddr_in *)addr;
	socket->opts |= SOCK_OPT_ADDR_BIND;

	return 0;
}

int 
q_listen(qapp_t app, int sockid, int backlog)
{
	struct tcp_listener *listener;
	int ret;
	int i;
	listeners_t listeners = get_global_ctx()->listeners;
	socket_table_t socket_table = get_global_ctx()->socket_table;
	socket_t socket;

	if (app) {
		TRACE_CHECKP("q_listen() was called @ Core %d\n", app->core_id);
	} else {
		TRACE_CHECKP("q_listen() was called\n");
	}
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	socket = &socket_table->sockets[sockid];


	if (socket->socktype == SOCK_TYPE_UNUSED) {
		TRACE_EXCP("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;

	}

	if (socket->socktype == SOCK_TYPE_STREAM) {
		socket->socktype = SOCK_TYPE_LISTENER;
	}
	
	if (socket->socktype != SOCK_TYPE_LISTENER) {
		TRACE_EXCP("Not a listening socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return -1;
	}

	if (backlog <= 0 || backlog > CONFIG.max_concurrency) {
		errno = EINVAL;
		return -1;
	}

	/* check whether we are already listening on the same port */
	if (listeners_search(listeners, socket->saddr.sin_port)) {
		errno = EADDRINUSE;
		return -1;
	}

	// TODO: use app->core_id instead of 0
	listener = (struct tcp_listener *)mempool_alloc_chunk(
			get_global_ctx()->mp_listener, 0);
	if (!listener) {
		/* errno set from the malloc() */
		return -1;
	}

	listener->sockid = sockid; 
	listener->backlog = backlog;
	listener->socket = socket;
	listener->socket->epoll |= Q_EPOLLIN;
	listener->port = ntohs(listener->socket->saddr.sin_port);
	listener->accept_point = 0;
	
	for (i=0; i<CONFIG.app_thread; i++) {
		streamq_init(&listener->acceptq[i],CONFIG.stack_thread, backlog);
	}
	
	socket->listener = listener;
	pthread_spin_lock(&listeners->insert_lock);
	ret = listeners_insert(listeners, listener);
	if (ret != SUCCESS) {
		// the listeners table is full
		mempool_free_chunk(get_global_ctx()->mp_listener, listener, 
				app->core_id);
		pthread_spin_unlock(&listeners->insert_lock);
		return -1;
	}
	pthread_spin_unlock(&listeners->insert_lock);

	return 0;
}

int 
q_accept(qapp_t app, int sockid, struct sockaddr *addr, socklen_t *addrlen)
{
	socket_t socket = NULL;
	socket_t listen_socket;
	listeners_t listeners = get_global_ctx()->listeners;
	socket_table_t socket_table = get_global_ctx()->socket_table;
	tcp_stream_t accepted = NULL;
	struct tcp_listener *listener;

	TRACE_CHECKP("q_accept() was called @ Core %d @ Server %d\n", 
			app->core_id, app->app_id);
	if (app->app_id >= CONFIG.app_thread) {
		TRACE_EXCP("Server id %d out of range.\n", app->app_id);
		errno = EBADF;
		return -1;
	}
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	/* requires listening socket */
	listen_socket = &socket_table->sockets[sockid];
	if (listen_socket->socktype != SOCK_TYPE_LISTENER) {
		errno = EINVAL;
		TRACE_EXCP("error socket type!\n");
		return -1;
	}

	listener = listen_socket->listener;

	/* dequeue from the acceptq without lock first */
	/* if nothing there, acquire lock and cond_wait */
	TRACE_CNCT("try to accept from acceptq %d\n", app->app_id);
	accepted = streamq_dequeue(&listener->acceptq[app->app_id]);
	if (!accepted) {
		//TODO:	non-blocking mode, if in block mode, switch the coroutin 
		//		context
		errno = EAGAIN;
		return -1;
	}

	if (!accepted->socket) {
//		socket = socket_alloc(app, SOCK_TYPE_STREAM);
		if (!socket) {
			TRACE_EXCP("Failed to create new socket!\n");
			/* TODO: destroy the stream */
			errno = ENFILE;
			return -1;
		}
	} else {
		socket = accepted->socket;
		socket->stream = accepted;
		accepted->socket = socket;
		accepted->id = socket->id;

		/* set socket parameters */
		socket->saddr.sin_family = AF_INET;
		socket->saddr.sin_port = accepted->dport;
		socket->saddr.sin_addr.s_addr = accepted->daddr;
	}
	//TODO: raise readevent if the accept_queue is not empty
	TRACE_CNCT("successfully accepted @ Stream %d\n", accepted->id);

	if (addr && addrlen) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = accepted->dport;
		addr_in->sin_addr.s_addr = accepted->daddr;
		*addrlen = sizeof(struct sockaddr_in);
	}

	DSTAT_ADD(get_global_ctx()->accepted_num[app->app_id], 1);
	return accepted->socket->id;
}

int
q_connect(qapp_t app, int sockid, const struct sockaddr *addr, 
		socklen_t addrlen)
{
	qstack_t qstack = NULL;;
	socket_t socket = NULL;
	int is_dyn_bound = FALSE;
	int ret, nif;
	int rss_core;

	TRACE_CHECKP("q_connect() was called @ Socket %d\n", sockid);
	TRACE_CNCT("q_connect() was called @ Socket %d\n", sockid);
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return ERROR;
	}
	socket = &get_global_ctx()->socket_table->sockets[sockid];

	if (socket->socktype == SOCK_TYPE_UNUSED) {
		TRACE_EXCP("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return FAILED;
	}
	
	if (socket->socktype != SOCK_TYPE_STREAM) {
		TRACE_EXCP("Not an end socket. id: %d\n", sockid);
		errno = ENOTSOCK;
		return FAILED;
	}

	if (!addr) {
		TRACE_EXCP("Socket %d: empty address!\n", sockid);
		errno = EFAULT;
		return ERROR;
	}

	/* we only allow bind() for AF_INET address */
	if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
		TRACE_EXCP("Socket %d: invalid argument!\n", sockid);
		errno = EAFNOSUPPORT;
		return FAILED;
	}

	if (socket->stream) {
		TRACE_EXCP("Socket %d: stream already exist!\n", sockid);
		if (socket->stream->state >= TCP_ST_ESTABLISHED) {
			errno = EISCONN;
		} else {
			errno = EALREADY;
		}
		return FAILED;
	}

	/* address binding */
	socket->daddr = *(struct sockaddr_in *)addr;
	if ((socket->opts & SOCK_OPT_ADDR_BIND) && 
	    socket->saddr.sin_port != INPORT_ANY &&
	    socket->saddr.sin_addr.s_addr != INADDR_ANY) {
		rss_core = get_rss_core(&socket->saddr, &socket->daddr);
		qstack = get_stack_context(rss_core);
	} else {
		TRACE_TODO();
	}

	ret = streamq_enqueue(&qstack->connect_queue, app->core_id, 
			(tcp_stream_t)socket);
	if (ret < 0) {
		TRACE_EXCP("Socket %d: failed to enqueue to conenct queue!\n", sockid);
		errno = EAGAIN;
		return FAILED;
	}

	/* nonblocking socket, return EINPROGRESS */
	// TODO: blocking mode support
	errno = EINPROGRESS;
	return SUCCESS;
}

mbuf_t
q_recv(qapp_t app, int sockid, char **buf, ssize_t *len, uint8_t flags)
{
	tcp_stream_t cur_stream;
	struct tcp_recv_vars *rcvvar;
	mbuf_t ret = NULL;
	uint32_t prev_rcv_wnd;
	qstack_t qstack;
	
	TRACE_CHECKP("q_recv() was called @ Core %d @ Socket %d\n", 
			app->core_id, sockid);
	DSTAT_ADD(get_global_ctx()->recv_called_num[app->app_id], 1);
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		*len = -1;
		errno = EBADF;
		return NULL;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	
    if (!cur_stream || 
			!(cur_stream->state >= TCP_ST_ESTABLISHED && 
			cur_stream->state <= TCP_ST_CLOSE_WAIT)) {
		errno = ENOTCONN;
		*len = -1;
		return NULL;
	}
	qstack = cur_stream->qstack;
	
	rcvvar = &cur_stream->rcvvar;
	/* if CLOSE_WAIT, return 0 if there is no payload */
	if (cur_stream->state == TCP_ST_CLOSE_WAIT) {
		TRACE_CLOSE("call recv() at CLOSE_WAIT @ Socket %d\n", sockid);
		if (rb_merged_len(&rcvvar->rcvbuf) == 0) {
			TRACE_CLOSE("return 0 at recv because of empty rb @ Socket %d\n",
					sockid);
			*len = 0;
			return NULL;
		}
	}
	
	ret = rb_get(app->core_id, cur_stream);
	if (ret) {
		rs_ts_add(ret->q_ts, REQ_ST_REQREAD);
		rs_ts_pass_to_stream(ret, cur_stream);

		ret->mbuf_state = MBUF_STATE_RREAD;
		*buf = mbuf_get_payload_ptr(ret);
		*len = ret->payload_len;
		DSTAT_ADD(get_global_ctx()->rmbuf_get_num[app->app_id], 1);
		TRACE_MBUF("mbuf %p received @ Stream %d, seq:%u len:%d priority:%d\n", 
				ret, cur_stream->id, ret->tcp_seq, *len, ret->priority);
	} else {
		*len = -1;
		errno = EAGAIN;
	}
	rcvvar->rcv_wnd = rcvvar->rcvbuf.size - rb_merged_len(&rcvvar->rcvbuf);

	// TODO: raise EPOLLIN if there is still data to be read or at CLOSE_WAIT
	return ret;
}

int 
q_close(qapp_t app, int sockid)
{
	int ret;
	TRACE_CLOSE("q_close() is called @ Socket %d\n", sockid);
	DSTAT_ADD(get_global_ctx()->close_called_num[app->app_id], 1);

	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	// now only stream socket can be closed
	if (get_global_ctx()->socket_table->sockets[sockid].socktype != 
			SOCK_TYPE_STREAM) {
		TRACE_EXCP("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	tcp_stream_t cur_stream = NULL;
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	if (cur_stream->state == TCP_ST_ESTABLISHED) {
		TRACE_CLOSE("q_close() is activly called "
				"by the application @ Stream %d\n", 
				cur_stream->id);
	}
	if (cur_stream->state == TCP_ST_ESTABLISHED) {
		TRACE_CLOSE("q_close() is activly called "
				"by the application @ Stream %d\n", 
				cur_stream->id);
	}
	ret = close_stream_socket(app, sockid);
	// socket should be freed at stack thread
//	socket_free(app->core_id, sockid);
	return ret;
}

int
q_socket_ioctl(qapp_t app, int sockid, int request, void *argp)
{
	socket_t socket;
	socket_table_t table = get_global_ctx()->socket_table;
	
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}

	/* only support stream socket */
	socket = &table->sockets[sockid];
	if (socket->socktype != SOCK_TYPE_STREAM &&
	    socket->socktype != SOCK_TYPE_LISTENER) {
		TRACE_EXCP("Invalid socket id: %d\n", sockid);
		errno = EBADF;
		return -1;
	}

	if (!argp) {
		errno = EFAULT;
		return -1;
	}

	if (request == FIONREAD) {
		tcp_stream_t cur_stream;
		rcv_buff_t rbuf;

		cur_stream = socket->stream;
		if (!cur_stream) {
			errno = EBADF;
			return -1;
		}
		rbuf = &cur_stream->rcvvar.rcvbuf;
		if (rbuf) {
			// not multi-thread-safe since they are not volatile
			*(int *)argp = rbuf->merged_next - rbuf->head_seq;
		} else {
			*(int *)argp = 0;
		}
	} else {
		errno = EINVAL;
		return -1;
	}

	return 0;
}



int
q_sendv(qapp_t app, int sockid, struct iovec *iov, int iovcnt, uint8_t flags)
{
	mbuf_t mbuf = NULL;
	uint32_t len = 0;
	uint32_t ret = 0;
	uint32_t offset = 0;
	int i;
	char *buff = NULL;

	mbuf = q_get_wmbuf(app, &buff, &len);
	if (!mbuf) {
		// TODO: set errno
		return -1;
	}
	
	for (i=0; i<iovcnt; i++) {
		if (offset+iov[i].iov_len > len) {
			// if one packet is full
			ret += q_send(app, sockid, mbuf, offset, flags);
			mbuf = q_get_wmbuf(app, &buff, &len);
			if (!mbuf)  {
				break;
			}
			offset = 0;
		}
		memcpy(buff+offset, iov[i].iov_base, iov[i].iov_len);
		offset += iov[i].iov_len;
	}
	ret += q_send(app, sockid, mbuf, offset, flags);
	return ret;
}

// announced in qstack.h
qapp_t
qapp_init(int core_id)
{
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
	CPU_SET(core_id, &cpus);	// add CPU core_id into cpu set "cpus"
	// bind the current thread with target core
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpus);
	
	return get_global_ctx()->app_contexts[core_id];
}

uint32_t
get_rss_core(struct sockaddr_in *saddr, struct sockaddr_in *daddr)
{
	union rte_thash_tuple tuple;
	uint32_t rss, ret;
	tuple.v4.dst_addr = ntohl(saddr->sin_addr.s_addr);
	tuple.v4.src_addr = ntohl(daddr->sin_addr.s_addr);
	tuple.v4.dport = ntohs(saddr->sin_port);
	tuple.v4.sport = ntohs(daddr->sin_port);
	//rss = rte_softrss_be((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, rss_key_be);
	rss = rte_softrss((uint32_t *)&tuple, RTE_THASH_V4_L4_LEN, default_rss_key);
	ret = rss % CONFIG.stack_thread;
	TRACE_CNCT("rss calculate: saddr=%lx, daddr=%lx, sport=%u, dport=%u, "
			"rss=%u, num_stacks=%u, ret=%u\n", 
			tuple.v4.src_addr, tuple.v4.dst_addr, 
			tuple.v4.sport, tuple.v4.dport, 
			rss, CONFIG.stack_thread, ret);
	return ret;
}
/******************************************************************************/
uint8_t
get_app_id(qapp_t app)
{
if(app)
	return app->app_id;
else
	TRACE_EXIT("Invalid qapp.\n");
}

uint8_t
get_core_id(qapp_t app)
{
	if(app)
		return app->core_id;
	else
		TRACE_EXIT("Invalid qapp.\n");
}

qapp_t
get_qapp_by_id(int core_id)
{
return get_core_context(core_id)->rt_ctx->qapp;
}

int
get_max_concurrency()
{
    return CONFIG.max_concurrency;
}

int
get_num_server()
{
    return CONFIG.app_thread;
}

struct qstack_conf*
qstack_getconf(char *cfg_file)
{
	int ret; 
	ret = load_configuration(cfg_file);		
	if(ret < 0)
		return NULL;
	else
		return &CONFIG;
}

/******************************************************************************/
/*----------------------------------------------------------------------------*/
