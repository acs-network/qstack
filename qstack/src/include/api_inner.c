#ifndef __API_INNER_C_
#define __API_INNER_C_
#include "qstack.h"

#ifdef INLINE_DISABLED 
#include "api.h"
#endif

#define QFLAG_SEND_HIGHPRI 		0x01

#ifndef INLINE_DISABLED 
static inline 
#endif
void host_ip_set(const char *addr)

{
	CONFIG.eths[0].ip_addr = inet_addr(addr);
}

#ifndef INLINE_DISABLED 
static inline 
#endif
mbuf_t
q_get_wmbuf(qapp_t app, uint8_t **buff, int *max_len)
{
	// TODO: app_id is better
	mbuf_t mbuf = io_get_wmbuf(app, buff, max_len, 1);
	return mbuf;
}

#ifndef INLINE_DISABLED
static inline 
#endif
void
q_free_mbuf(qapp_t app, mbuf_t mbuf)
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
		mbuf_set_op(mbuf, MBUF_OP_RCV_UFREE, app->core_id);
		DSTAT_ADD(get_global_ctx()->request_freed[app->app_id], 1);
		mbuf_free(app->core_id, mbuf);
	} else {
		TRACE_EXCP("try to free empty mbuf at Core %d!\n", app->core_id);
	}
}




#ifndef INLINE_DISABLED
static inline 
#endif
void
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




#ifndef INLINE_DISABLED
static inline 
#endif
void
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


#ifndef INLINE_DISABLED
static inline 
#endif
int
q_send(qapp_t app, int sockid, mbuf_t mbuf, uint32_t len, uint8_t flags)
{
	TRACE_CHECKP("q_send() was called @ Core %d @ Socket %d\n", 
			app->core_id, sockid);
	DSTAT_ADD(get_global_ctx()->write_called_num[app->app_id], 1);
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

#endif
