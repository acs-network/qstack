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
 *   8. q_write(): send an mbuf to send buffer \n
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
#include "qstack.h"
#include "tcp_out.h"
#include "io_module.h"
/*----------------------------------------------------------------------------*/
#ifdef __cplusplus
// TODO: this is commented because of folding, cancle the fold before release
//extern "C" {
#endif
/******************************************************************************/
/* global macros */
#define QFLAG_SEND_HIGHPRI 		0x01

#ifndef INPORT_ANY
	#define INPORT_ANY  (uint16_t)0
#endif
/******************************************************************************/
/* function declarations */
/*----------------------------------------------------------------------------*/
extern int(*driver_pri_filter)(mbuf_t mbuf);
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
static inline mbuf_t
q_get_wmbuf(qapp_t app, uint8_t **buff, int *max_len)
{
	// TODO: app_id is better
	mbuf_t mbuf = io_get_wmbuf(app->core_id, buff, max_len, 1);
	return mbuf;
}

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
static inline int
q_write(qapp_t app, int sockid, mbuf_t mbuf, uint32_t len, uint8_t flags)
{
	TRACE_CHECKP("q_write() was called @ Core %d @ Socket %d\n", 
			app->core_id, sockid);
	DSTAT_ADD(get_global_ctx()->write_called_num[app->core_id], 1);
	tcp_stream_t cur_stream; 
	int ret = -2;
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return -1;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	if (!cur_stream) {
		TRACE_EXCP("no available stream in socket!");
		errno = EBADF;
		return -1;
	}
    
	if (flags & QFLAG_SEND_HIGHPRI) {
		mbuf->priority = 1;
	} else {
		mbuf->priority = 0;
	}
	
	{
		ret = _q_tcp_send(app->core_id, sockid, mbuf, len, flags);
	}
	return ret;
}

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
q_writev(qapp_t app, int sockid, struct iovec *iov, int iovcnt, uint8_t flags);

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

/**
 * free the used mbuf 
 *
 * @param core_id	core_id of the core where the function is called
 * @param mbuf		target mbuf to be freed
 *
 * @return null
 */
static inline void
q_free_mbuf(int core_id, mbuf_t mbuf)
{
	if (mbuf) {
#ifdef PRIORITY_RECV_BUFF
		if (mbuf->priority && mbuf->holding) {
			// the high-pri mbuf should be recieved by user only once, and 
			// should always not be freed
			// TODO: may not be multi-thread safe
			mbuf->holding = 0;
			return;
		}
#endif
		mbuf_set_op(mbuf, MBUF_OP_RCV_UFREE, core_id);
		DSTAT_ADD(get_global_ctx()->request_freed[core_id], 1);
		mbuf_free(core_id, mbuf);
	} else {
		TRACE_EXCP("try to free empty mbuf at Core %d!\n", core_id);
	}
}

static inline uint32_t
q_virtual_process(uint64_t delay)
{
#if VIRTUAL_TASK_DELAY_MODE
	uint64_t i;
	uint32_t a, b;
	uint64_t loop_time = delay * 23 / 15;

	a = 1;
	b = 1;
	for (i=0; i<loop_time; i++) {
		b = a + b;
		b = a + b;
		a = b - a;
		b = b - a;
	}
	return a;
#else
	return 0;
#endif
}

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
static inline void
q_sockset_req_head(int sockid, int (*func)(char*))
{
	tcp_stream_t cur_stream;
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	
	set_req_head(cur_stream, func);
}

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
static inline void
q_sockset_req_high(int sockid, int (*func)(char*))
{
	tcp_stream_t cur_stream;
	if (sockid < 0 || sockid >= CONFIG.max_concurrency) {
		TRACE_EXCP("Socket id %d out of range.\n", sockid);
		errno = EBADF;
		return;
	}
	cur_stream = get_global_ctx()->socket_table->sockets[sockid].stream;
	
	set_req_high(cur_stream, func);
}

/**
 * register the priority filter function for pakcet-level priority
 * classification at driver layer
 *
 * @param func		the filter function
 *
 * @return NULL
 */
static inline void
q_register_pkt_filter(int(*func)(mbuf_t))
{
	driver_pri_filter = func;
}

/**
 * set host IP address
 *
 * @param addr 	host ip address (in "xxx.xxx.xxx.xxx" type)
 *
 * @return NULL
 */
static inline void
host_ip_set(const char *addr)
{
	CONFIG.eths[0].ip_addr = inet_addr(addr);
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
#ifdef __cplusplus
//};
#endif
#endif //#ifdef __API_H_
