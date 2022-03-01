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
 * @file socket.h 
 * @brief structures and functions for sockets used in stack
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.6.25
 * @version 0.1
 * @detail Function list: \n
 *   1. socket_table_init(): alloc and init socket table\n
 *   2.	socket_alloc(): allocate and init a socket\n
 *   3. socket_free(): free a socket\n
 *   4. listeners_init(): alloc and init tcp_listeners for global context\n
 *   5. listeners_search(): find the listener with the dest port\n
 *   6. listeners_insert(): insert a listener into the listener_table\n
 *   7. listeners_remove(): remove a listener from the listener_table\n
 *   8. get_qe_from_socket(): get qepoll handle the socket is listening to\n
 *   9. get_epoll_from_socket(): get the epoll listening flags of the socket\n
 *   10. bind_socket_with_qe(): make a socket listen to a qepoll handle\n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.25 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __SOCKET_H_
#define __SOCKET_H_
/******************************************************************************/
/* forward declarations */
struct socket;
struct socket_table;
struct tcp_listeners;
typedef struct socket *socket_t;
typedef struct socket_table *socket_table_t;
typedef struct tcp_listeners *listeners_t;
/******************************************************************************/
#include "stream_queue.h"
#include "universal.h"
#include "qstack.h"
/******************************************************************************/
/* global macros */
#define MAX_SOCKET_NUM	MAX_FLOW_NUM		// socket used for pasitive open
#define PUB_SOCKET_NUM	0	// public free_socket pool, for positive open 
//#define MAX_SOCKET_NUM	100000
#define MAX_SOCKET_PSTACK	(MAX_SOCKET_NUM)/(CONFIG.num_stacks)
/******************************************************************************/
/* data structures */
enum socket_opts
{
	SOCK_OPT_NONBLOCK		= 0x01,
	SOCK_OPT_ADDR_BIND		= 0x02, 
};
enum socket_type
{
	SOCK_TYPE_UNUSED, 
	SOCK_TYPE_STREAM, 
//	SOCK_TYPE_PROXY, 
	SOCK_TYPE_LISTENER, 
//	SOCK_TYPE_EPOLL, 
//	SOCK_TYPE_PIPE, 
};

/*----------------------------------------------------------------------------*/
struct tcp_listener
{
	int sockid;
	uint16_t port;
	uint16_t is_ssl:1;
	socket_t socket;

	int backlog;
	int accept_point;		// round robin for accept event
	tcp_stream_queue acceptq[MAX_SERVER_NUM];
	q_SSL_CTX *ssl_ctx;
//	TAILQ_ENTRY(tcp_listener) he_link;	/* hash table entry link */
};

struct tcp_listeners
{
	pthread_spinlock_t insert_lock;
	struct tcp_listener *table[MAX_SOCKET_NUM];
	uint32_t num;
};

struct socket
{
	int id;
	uint8_t socktype;
	uint8_t default_app;
	uint32_t opts;
	uint32_t epoll;						///< registered events
//	qepoll_data_t ep_data;
	volatile qepoll_t qe;	///< the event handle listening to

	union {
		struct tcp_stream *stream;
		struct tcp_listener *listener;
	};

	TAILQ_ENTRY (socket) free_link; ///< used for free socket link list
	struct sockaddr_in saddr;
	struct sockaddr_in daddr;
};

struct socket_table
{
	socket_t sockets;						///< array of all the sockets
	TAILQ_HEAD(, socket) free_socket[MAX_STACK_NUM];
	TAILQ_HEAD(, socket) free_socket_public;
	int free_num[MAX_STACK_NUM];
	int free_public_num;
};

/******************************************************************************/
/* functions */
/**
 * get the qepoll handle of the socket
 *
 * @param sockid	socket fd
 *
 * @return
 * 	return the qepoll handle the socket is listening to;
 * 	return NULL if the socket is not related to any qepoll handle.
 */
qepoll_t
get_qe_from_socket(int sockid);

/**
 * bind a socket with qepoll handle
 *
 * @param sockid	socket fd of the target socket
 * @param qe		target qepoll handle
 *
 * @return
 * 	return SUCCESS if successfully bind the qe to the socket;
 * 	otherwise return FALSE
 */
int
bind_socket_with_qe(int sockid, qepoll_t qe);

/**
 * get the qepoll flags of listening type from the socket
 *
 * @param sockid	socket fd
 *
 * @return
 * 	get the epoll listening flags of the socket
 */
uint32_t *
get_epoll_from_socket(int sockid);

static inline uint32_t 
get_sock_pool_map(qapp_t app)
{
#if SOCK_ALLOC_MAP == SOCK_ALLOC_STACK_ONLY
	return 0;
#elif SOCK_ALLOC_MAP == SOCK_ALLOC_APP_ONLY
	return app->app_id;
#elif SOCK_ALLOC_MAP == SOCK_ALLOC_ALL
	return app->core_id;
#endif
}
/*----------------------------------------------------------------------------*/
/**
 * alloc and init socket table
 *
 * @param table 		target socket table
 *
 * @return null
 */
socket_table_t 
socket_table_init();

/**
 * alloc and init a socket for user
 *
 * @param core_id		core_id of the core try to alloc socket
 * @param socktype		socket type, see enum socket_type 
 *
 * @return 
 * 	return the allocated socket instance if success; otherwise return NULL
 * @note
 * 	currently this function should always called by stack thread
 */
socket_t 
socket_alloc(int core_id, int socktype);

/**
 * free a socket
 *
 * @param core_id	the core on which the function is called
 * @param sockid	id of the target socket
 *
 * @return null
 */
void 
socket_free(int core_id, int sockid);

/**
 * alloc and init tcp_listeners for global context
 *
 * @return 
 * 	return the allocated tcp_listeners if success; otherwise return NULL
 */
static inline listeners_t
listeners_init()
{
	listeners_t listeners = (listeners_t)calloc(1, 
			sizeof(struct tcp_listeners));
	pthread_spin_init(&listeners->insert_lock, PTHREAD_PROCESS_PRIVATE);
	listeners->num = 0;
	return listeners;
}

/**
 * search the listener_table to find the listener with the dest port
 *
 * @param listeners 	target listener table
 * @param port 			dest server port
 *
 * @return
 * 	return the listener if found; otherwise return NULL
 */
struct tcp_listener*
listeners_search(listeners_t listeners, uint16_t port);

/**
 * insert a listener into the listener_table
 *
 * @param listeners 	target listener table
 * @param listener		target listener
 *
 * @return
 * 	return SUCCESS if successfully insert; otherwise return FAILED or ERROR
 */
int
listeners_insert(listeners_t listeners, struct tcp_listener *listener);

/**
 * remove a listener from the listener_table
 *
 * @param listeners 	target listener table
 * @param listener		target listener
 *
 * @return
 * 	return SUCCESS if successfully remove; otherwise return FAILED or ERROR
 */
int 
listeners_remove(listeners_t listeners, struct tcp_listener *listener);

/**
 * get the pending event waiting in the steam, which came when the socket was 
 * not related to qepoll
 *
 * @param sockid	socket id
 *
 * @return
 * 	return one event if there is any pending event in the socket;
 * 	otherwise return NULL
 */
struct qepoll_event *
get_pending_event(int sockid);
/*----------------------------------------------------------------------------*/
int
get_stackid_from_sockid(uint32_t sockid);

void
socket_scan();
/******************************************************************************/
#endif //#ifdef __SOCKET_H_
