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
 * @file api.h 
 * @brief APIs for applications
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.9.6
 * @version 0.1
 * @detail Function list: \n
 *   1. q_socket(): alloc a free socket and get sockid \n
 *   2. q_bind(): bind a socket with target address \n
 *   3. q_listen(): create a listener for a socket \n
 *   4. q_accept(): accept connections from listener \n
 *   5. q_connect(): try to connect to a server \n
 *   6. q_recv(): receive an mbuf from receive buffer \n
 *   7. q_get_wmbuf(): get a free mbuf to be writen and sent out \n
 *   8. q_send(): send an mbuf to send buffer \n
 *   9. q_writev(): send an iovec array to send buffer \n
 *   10. q_close(): close connection on the socket \n
 *   11. q_socket_ioctl(): set socket configuration or get socket states \n
 *   12. q_free_mbuf(): free the read mbuf received from socket \n
 *   13. q_sockset_req_head(): check the message head \n
 *   14. q_sockset_req_high(): check the message priority \n
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
#ifndef __API_H_
#define __API_H_
/******************************************************************************/
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef INLINE_DISABLED 
//complie inner app , inline
#include "api_inner.c"
#endif
/*----------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/* global macros */
#define QFLAG_SEND_HIGHPRI 		0x01
#ifndef INPORT_ANY
	#define INPORT_ANY  (uint16_t)0
#endif


/******************************************************************************/
/* global data structures
***************************************************************************** */
#ifndef __QSTACK_H_
struct qapp;

typedef struct qapp*  qapp_t;


/**
 * create an application thread, and pin it to the target core
 *
 * @param core_id		the core on which the application is goning to run
 * @app_handle[out]		the handle of created application thread
 * @param app_func		the entry function of application
 * @param args			the args for app_func
 *
 * @return
 * 	return SUCCESS if success; otherwise return FALSE or ERROR
 * @note
 *  input app_handle with NULL if don't need a qapp return
 */
#ifndef __RUNTIME_MGT_H_
typedef int (* app_func_t)(void *);
#endif
int
qstack_create_app(int core_id, qapp_t *app_handle, app_func_t app_func,
                  void *args);


void
qstack_init(int stack_num);


#endif

#ifndef __MBUF_H_
//struct mbuf;
//typedef struct mbuf*  mbuf_t;
typedef struct rte_mbuf* mbuf_t;
#endif



#ifndef __ARP_H_
void
sarp_set(const char *dip, uint8_t mask, const char *haddr);
#endif

extern int(*driver_pri_filter)(mbuf_t mbuf);



/******************************************************************************/
/* EPOLL-related data structures
***************************************************************************** */
/*----------------------------------------------------------------------------*/
#ifndef __QEPOLL_H_

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

/* Structure for storing qepoll event data.*/
union qepoll_data
{
    void *ptr;
    int sockid;
    uint32_t u32;
    uint64_t u64;
};


typedef union qepoll_data qepoll_data_t;
/* Structure for storing qepoll event infomation.*/
#define QVDEB
struct qepoll_event
{
    uint8_t pri;
    uint8_t core;
    uint8_t dire;
    uint8_t nature;
    uint32_t events;
    int apid;
    int timeout;
    int sockid;
    qepoll_data_t data;
#ifdef QVDEB
    int      flow_st;
    uint64_t time_sp;
#endif
    struct qepoll_event *next;
};

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
q_init_manager(int stack, int server);

/**
 * initialize qepoll struct and allocate memory
 * @param[in]	 size  	max size of queue
 * @return		 efd of the event; -1 if the create operation fails
 * @ref 		 qepoll.h
 */
int
qepoll_create(qapp_t app, uint32_t size);

/**
 * wait for events in polling/coroutine mode
 * @param[in]	 app  		max size of queue
 * @param[in]	 events  	qepoll events data structures
 * @param[in]	 maxevents  the maximum number of the events can be fetched
 * @param[in]	 timeout  	the timeout of qepoll wait for events
 * @return		 events number to be processed; -1 means timeout
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int
qepoll_wait(qapp_t app, struct qepoll_event *events, int maxevents, int timeout);
  
/**
 * wait epoll events with explicit priority
 *
 * @param[in]	 efd  		max size of queue
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
 * @param[in]    app_id     app id
 * @param[in]	 efd		qepoll file descriptor id
 * @param[in]	 op 		the operation for modify events states
 * @param[in]	 sockid		socket id of the event
 * @param[in]	 events		qepoll events data structures
 * @return		 events number to be processed; -1 means timeout
 * @ref 		 qepoll.h
 * @see
 * @note
 */
int
qepoll_ctl(int app_id, int fd, int op, unsigned long long allNow, struct qepoll_event *event);

struct qepoll_event*
eventmgt_get(int efd, struct event_mgt *mgt);

void
eventmgt_init(evmgt_t evmgt, int maxsize, int lb);


#endif



/******************************************************************************/
/* global function declarations
***************************************************************************** */
uint8_t
get_app_id(qapp_t app);

uint8_t
get_core_id(qapp_t app);

qapp_t
get_qapp_by_id(int core_id);

int
get_max_concurrency();

int
get_num_server();

struct qstack_config*
qstack_getconf(char *cfg_file);

static inline void
q_register_pkt_filter(int(*func)(mbuf_t))
{
	driver_pri_filter = func;
}

/**
 * get rss result according to the src/dst ip_addr and tcp_port
 *
 * @param saddr		source ip address and tcp port, in ipv4 format
 * @param daddr		dest ip address and tcp port, in ipv4 format
 *
 * @return
 * 	the id of the stack this connection belongs to
 */
uint32_t
get_rss_core(struct sockaddr_in *saddr, struct sockaddr_in *daddr);

/******************************************************************************/
/* EPOLL-related function declarations
***************************************************************************** */



/******************************************************************************/
/* function declarations */
/*----------------------------------------------------------------------------*/
// socket process
/**
 * create a new socket
 *
 * @param app 			application context
 * @param domain		communication domain, protocol family, see 
 * 						'#man socket', usually use AF_INET for ipv4
 * @param type			socket type, usually use SOCK_STREAM as default
 * @param protocol		usually use (0) as default
 *
 * @return
 * 	return sockid of the new socket if success; otherwise return (-1)
 */
uint32_t  
q_socket(qapp_t app, int domain, int type, int protocol);

/**
 * bind an address to a socket
 *
 * @param app 			application context
 * @param sockid 		socket id
 * @param addr			source address (ip addr and tcp port)
 * @param addrlen		length of addr
 *
 * @return
 * 	return (0) if success; otherwise return -1
 */
int 
q_bind(qapp_t app, int sockid, const struct sockaddr *addr, socklen_t addrlen);

/**
 * set a socket as listener and start listening
 *
 * @param app 			application context
 * @param sockid 		socket id
 * @param backlog		max length of backlog queue (>0)
 *
 * @return
 * 	return 0 if success; otherwise return ERROR
 */
int 
q_listen(qapp_t app, int sockid, int backlog);

/**
 * accept connection requests from listener
 *
 * @param app 			application context
 * @param sockid 		listener's socket id
 * @param addr[out]		return the address of the accepted socket
 * @param addrlen[out]	return the length of address of the accepted socket
 *
 * @return
 * 	return the sockid of the accepted socket, if failed, return -1
 */
int 
q_accept(qapp_t app, int sockid, struct sockaddr *addr, socklen_t *addrlen);

/**
 * try connect a remote server
 *
 * @param app 			application context
 * @param sockid 		socket id
 * @param addr			dest address of the connection
 * @param addrlen		length of addr
 *
 * @return
 * 	return SUCCESS (non-block, can't establish the connection right now) if 
 * 	success; otherwise renturn FAILED or ERROR, which is different to POSIC API
 */
int
q_connect(qapp_t app, int sockid, const struct sockaddr *addr, 
		socklen_t addrlen);

/**
 * receive an mbuf from rcv_buff
 *
 * @param app 			application context
 * @param sockid 		socket id
 * @param[out] buf 		return the pointer to the readalbe payload
 * @param[out] len 		return the length of readable in returned mbuf
 * @param flags 		read flags
 *
 * @return 
 * 	return the mbuf from rcv_buff is SUCCESS; otherwise return NULL
 * 	return -1 in len if the buffer is empty and errno=EAGAIN
 * 	
 * @note
 * 	the mbuf should be freed after having been read
 */
mbuf_t
q_recv(qapp_t app, int sockid, char **buf, ssize_t *len, uint8_t flags);


/**
 * set host IP address
 *
 * @param addr 	host ip address (in "xxx.xxx.xxx.xxx" type)
 *
 * @return NULL
 */

/*
#ifndef __BBB__
inline 
#endif
*/

//static inline
void host_ip_set(const char *addr);

/**
 * get a free mbuf to be writen and sent out
 *
 * @param app 			application context
 * @param[out] buff 	return the pointer pointing to position to write tcp 
 * 	payload in the returned mbuf
 * @param[out] 			max_len max length of payload to be writen in
 *
 * @return
 * 	return a writable mbuf if success; otherwise return NULL
 */

mbuf_t
q_get_wmbuf(qapp_t app, uint8_t **buff, int *max_len);


/**
 * free the used mbuf 
 *
 * @param app	    application context where the function is called
 * @param mbuf		target mbuf to be freed
 *
 * @return null
 */
void
q_free_mbuf(qapp_t app, mbuf_t mbuf);

/**
 * set the function to detect if a packet is the head of a message
 *
 * @param sockid		sockid of target socket
 * @param func			pointer of the requst head detect function
 *
 * @return null
 *
 * @note
 *  The function input a string with type of char* (usually the start of an 
 *  mbuf payload). The function return TRUE if the string do be the head of a 
 *  message, otherwith return FALSE.
 */

void
q_sockset_req_head(int sockid, int (*func)(char*));

/**
 * set the function to check if a message is with high priority
 *
 * @param sockid		sockid of the target socket
 * @param func			pointer of the high-priority check function
 *
 * @return null
 *
 * @note
 *  The function input a message with type of char*. The function return TRUE 
 *  if the message is with high priority, otherwith return FALSE.
 * @note
 * 	If the req_head check function is not set, the high-priority check function 
 * 	will check every packet's payload in default.
 */

void
q_sockset_req_high(int sockid, int (*func)(char*));

/**
 * send an mbuf to snd_buff
 *
 * @param app 		application context
 * @param sockid 	socket id
 * @param mbuf 		target mbuf to be sent
 * @param len 		tcp payload length writen in the mbuf
 * @param flags 	control flags:
 * 						QFLAG_SEND_HIGHPRI: high priority packet
 *
 * @return
 * 	return size of bytes writen to snd_buff, which is always equal to the param 
 * 	"len", and return FAILED(0) if failed to add mbuf to snd_buff, or ERROR(-1) 
 * 	if received unexpected parameters.
 * @note
 * 	the mbuf to be sent should be allocated by calling q_get_wmbuf()
 */

int
q_send(qapp_t app, int sockid, mbuf_t mbuf, uint32_t len, uint8_t flags);

//static inline 


/**
 * send data with iovec
 *
 * @param app		application context
 * @param sockid	socket id
 * @param iov		iovec array
 * @param iovcnt	size of iovec array
 * @param flags 	control flags:
 * 						QFLAG_SEND_HIGHPRI: high priority packet
 *
 * @return
 * 	return the number of bytes writen to the send buffer, 
 * 	return -1 if failed, and set ERRNO
 */
int
q_sendv(qapp_t app, int sockid, struct iovec *iov, int iovcnt, uint8_t flags);

/** 
 * close the socket
 *
 * @param app		application context
 * @param sockid	socket id
 *
 * @return
 * 	return 0 if success; otherwise return -1 and errno
 */
int 
q_close(qapp_t app, int sockid);

/**
 * similar to ioctlsocket(), control the socket configurations and get socket 
 * states
 *
 * @param app		application context
 * @param sockid	socket id
 * @param request	control command
 * @param argp		pointer to the arguments
 *
 * @return
 * 	return 0 if success; otherwise return -1 and error
 *
 * @note
 * 	now only FIONREAD is avaliable;
 */
int 
q_socket_ioctl(qapp_t app, int sockid, int request, void *argp);


/******************************************************************************/
#ifdef __cplusplus
};
#endif
#endif //#ifdef __API_H_
