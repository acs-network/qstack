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
#define _LARGEFILE64_SOURCE

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/syscall.h> 

#include <debug.h>
#include <qstack.h>
#include <api.h>

//#define HTTP_MODE

#ifdef HAVE_VERSION_H
# include "versionstamp.h"
#else
# define REPO_VERSION ""
#endif

//#define MAX_FLOW_NUM  (5000000)

#define RCVBUF_SIZE (2*1024)
#define SNDBUF_SIZE (8*1024)

#define MAX_EVENTS (MAX_FLOW_NUM * 1)
#define MAX_HIGH_EVENTS 64
#define MAX_LOW_EVENTS 128

#define HTTP_HEADER_LEN 1024
#define URL_LEN 128

#define MAX_CPUS 24
#define MAX_FILES 30

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef ERROR
#define ERROR (-1)
#endif

#define HT_SUPPORT FALSE

#include <sched.h>

//add by songhui
uint64_t requests;
uint64_t responces;
uint32_t task_delay_h;
uint32_t task_delay_l;

char html[] =
"HTTP/1.1 200 OK\r\n"
"Server: F-Stack\r\n"
"Date: Sat, 25 Feb 2017 09:26:33 GMT\r\n"
"Content-Type: text/html\r\n"
"Content-Length: 11\r\n"
"Last-Modified: Tue, 21 Feb 2017 09:44:03 GMT\r\n"
"Connection: keep-alive\r\n"
"\r\n"
"hello world";

qapp_t qapp_thread; 
/*----------------------------------------------------------------------------*/
struct file_cache
{
	char name[128];
	char fullname[256];
	uint64_t size;
	char *file;
};
/*----------------------------------------------------------------------------*/
struct server_vars
{
	char request[HTTP_HEADER_LEN];
	int recv_len;
	int request_len;
	long int total_read, total_sent;
	uint8_t done;
	uint8_t rspheader_sent;
	uint8_t keep_alive;

	int fidx;						// file cache index
	char fname[128];				// file name
	long int fsize;					// file size
};
/*----------------------------------------------------------------------------*/
struct thread_context
{
	qmag_t mctx;
	qapp_t qapp;
	int core;
	int ep;
	struct server_vars *svars;
};
/*----------------------------------------------------------------------------*/
struct sthread_args{
	int id;
    int core;
	int listener;
    struct thread_context *ctx;
};
/*----------------------------------------------------------------------------*/
struct app_buffer{
	int id;
    int core;    
};
/*----------------------------------------------------------------------------*/
static int num_cores;
static int core_limit;
static int nb_processors;
static pthread_t app_thread[MAX_CPUS];
static struct sthread_args server_thread[MAX_CPUS];
static int done[MAX_CPUS];
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
int 
redis_packet_pri_filter(mbuf_t mbuf)
{
	char *payload = mbuf_get_tcp_ptr(mbuf) + 32;
	return (mbuf->pkt_len>80 && payload[5] == 0x01);
}
/*----------------------------------------------------------------------------*/
void 
PrintStats() 
{
	uint64_t now,prev = 0;
	while(1)
	{    
		print_network_state();
	    /*if ((now - prev) > 1000 * 1000) {
			prev = now;
		                
		    uint64_t avg_redis_time;
		    uint64_t total_redis_time = 0;
		    int i;
		    int j, k;
		    uint64_t temp;
		    
		    for (i = 0; i < core_limit; i ++) {
    
				print_network_state();
		
			}
		}*/
	}
}
/*----------------------------------------------------------------------------*/
static char *
StatusCodeToString(int scode)
{
	switch (scode) {
		case 200:
			return "OK";
			break;

		case 404:
			return "Not Found";
			break;
	}

	return NULL;
}
/*----------------------------------------------------------------------------*/
void
CleanServerVariable(struct server_vars *sv)
{
	sv->recv_len = 0;
	sv->request_len = 0;
	sv->total_read = 0;
	sv->total_sent = 0;
	sv->done = 0;
	sv->rspheader_sent = 0;
	sv->keep_alive = 0;
}
/*----------------------------------------------------------------------------*/
void 
CloseConnection(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
//	qepoll_ctl(ctx->qapp->app_id, sockid, Q_EPOLL_CTL_DEL, GetTickMS(), NULL);
	qepoll_ctl(0, sockid, Q_EPOLL_CTL_DEL, GetTickMS(), NULL);
	//mtcp_close(ctx->mctx, sockid);
}
/*----------------------------------------------------------------------------*/
static int 
SendUntilAvailable(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
	/*int ret;
	int sent;
	int len;

	if (sv->done || !sv->rspheader_sent) {
		return 0;
	}

	sent = 0;
	ret = 1;
	while (ret > 0) {
		len = MIN(SNDBUF_SIZE, sv->fsize - sv->total_sent);
		if (len <= 0) {
			break;
		}
		ret = mtcp_write(ctx->mctx, sockid,  
		fcache[sv->fidx].file + sv->total_sent, len);
		if (ret < 0) {
			TRACE_APP("Connection closed with client.\n");
			break;
		}
		TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, ret);
		sent += ret;
		sv->total_sent += ret;
	}

	if (sv->total_sent >= fcache[sv->fidx].size) {
		struct mtcp_epoll_event ev;
		sv->done = TRUE;
		finished++;

		if (sv->keep_alive) {
			// if keep-alive connection, wait for the incoming request /
			ev.events = MTCP_EPOLLIN;
			ev.data.sockid = sockid;
			mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

			CleanServerVariable(sv);
		} else {
			// else, close connection /
			CloseConnection(ctx, sockid, sv);
		}
	}

	return sent;
	*/
}
/*----------------------------------------------------------------------------*/
static int 
HandleReadEvent(struct thread_context *ctx, int sockid, struct server_vars *sv, int pri, int core_id)
{
	struct qepoll_event ev;
	char *buf;
	char *response;
	time_t t_now;
	char t_str[128];
	mbuf_t mbuf;
	mbuf_t send_mbuf;
	int i, id, ret;
	uint32_t len, wlen, sndlen; 
    
	id = ctx->qapp->app_id;
	do {
		/* recieve request */
		mbuf = q_recv(ctx->qapp, sockid, &buf, &len, 0);
		if (!len) {
			TRACE_EXCP("try to recieve at a closed stream @ Socket %d\n", sockid);
			q_close(ctx->qapp, sockid);
		}
		if (!mbuf) {
			//fprintf(stderr, "error reading\n");
			return len;
		}
//		mbuf_print_detail(mbuf);

#ifndef ECHO_RESPONSE_ALL
		if (buf[5]==0x01) 
#endif
		{
			/* allocate mbuf for zero-copy send */
			send_mbuf = q_get_wmbuf(ctx->qapp, &response, &sndlen);
			if (!send_mbuf) {
				TRACE_EXCP("failed to get uwmbuf @ Socket %d @ Core %d\n", 
						sockid, ctx->qapp->core_id);
				return len;
			}
#ifndef HTTP_MODE 
			memcpy(response, buf, len);
			response[6] = 0x3;
			/* loop with given running time to emulate application processing */
			if (buf[5] == 0x0) {
				response[8] = q_virtual_process(task_delay_l);
			} else if (buf[5] == 0x1) {
				response[8] = q_virtual_process(task_delay_h);
			}
#else
			sprintf(response, "%s", html);
#endif   
	
			rs_ts_pass(mbuf, send_mbuf);	// pass rs_ts timestamp for 
//			mbuf_print_detail(send_mbuf);
	
			/* send out the prepared response mbuf */
			ret = q_send(ctx->qapp, sockid, send_mbuf, len, pri);
			if (ret != len) {
				TRACE_EXCP("q_send() failed @Socket %u, "
						"ret:%d, errno: %d\n", sockid, ret, errno);
			}
		}

		/* free the received mbuf */
		q_free_mbuf(core_id, mbuf);
	} while (0);

	CleanServerVariable(sv);
	/*ev.events = Q_EPOLLIN;
	ev.sockid = sockid;
	qepoll_ctl(ctx->core, Q_EPOLL_CTL_MOD, GetTickMS(), &ev);*/                      
	
	return len;
}

/*----------------------------------------------------------------------------*/
int 
CreateListeningSocket(struct thread_context *ctx)
{
	int listener;
	struct qepoll_event *ev;
	struct sockaddr_in saddr;
	int ret;

	/* create socket and set it as nonblocking */
	listener = q_socket(ctx->qapp, AF_INET, SOCK_TYPE_STREAM, 0);
	if (listener < 0) {
		TRACE_ERR("Failed to create listening socket!\n");
		return -1;
	}

	/* bind to port 80 */
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(80);
	ret = q_bind(ctx->qapp, listener, 
			(struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		TRACE_ERR("Failed to bind to the listening socket!\n");
		return -1;
	}

	/* listen (backlog: 4K) */
	ret = q_listen(ctx->qapp, listener, 10000);
	if (ret < 0) {
		TRACE_ERR("mtcp_listen() failed!\n");
		return -1;
	}

	return listener;
}

/*----------------------------------------------------------------------------*/
int
func_check_req_high(char *message)
{
	return (message[5]==0x01);
}

int
AcceptConnection(struct thread_context *ctx, int listener)
{
	struct server_vars *sv;
	struct qepoll_event ev;
	unsigned long long now;
	int c;

	c = q_accept(ctx->qapp, listener, NULL, NULL);
	

	if (c >= 0) {
		if (c >= MAX_FLOW_NUM) {
            TRACE_EXIT("%d larger than %d\n", c, MAX_FLOW_NUM);
			TRACE_EXCP("Invalid socket id %d.\n", c);
			return -1;
		}
		q_sockset_req_high(c, func_check_req_high);

		sv = &ctx->svars[c];
		CleanServerVariable(sv);
		TRACE_CNCT("New connection %d accepted.\n", c);
		/*ev = CreateQevent(0, ctx->core, c);
		ev->events = Q_EPOLLIN;
		now = GetTickMS();*/
		ev.events = Q_EPOLLIN;
		qepoll_ctl(ctx->qapp->app_id, c, Q_EPOLL_CTL_ADD, -1, &ev);
		TRACE_CNCT("Socket %d registered.\n", c);

	} else {
		TRACE_EXCP("q_accept() error on socket %d.\n", c);
	}

	return c;
}

/*----------------------------------------------------------------------------*/
void *
RunServerThread(void *arg)
{
	struct sthread_args server_thread = *(struct sthread_args *)arg;
	int core = server_thread.core;
	struct thread_context *ctx = server_thread.ctx;
	int listener;
	struct qepoll_event *events;
	int nevents;
	int i, ret;
	struct qepoll_event *ev;
	unsigned long long now, prev;

	ctx->qapp = get_core_context(core)->rt_ctx->qapp;
	int id = ctx->qapp->app_id;
              
	if (!ctx) {
		TRACE_ERR("Failed to initialize server thread.\n");
		return NULL;
	}
	listener = server_thread.listener;
	struct event_mgt evmgt;
	eventmgt_init(&evmgt, MAX_HIGH_EVENTS, MAX_LOW_EVENTS);
	TRACE_LOG("====================\napp %d start at core %d, pid:%d\n", 
			ctx->qapp->app_id, core, syscall(SYS_gettid));
	
	while (!done[core]) {
		/* get one event if avaiable, otherwise yield to other threads */
		while ((ev = eventmgt_get(id, &evmgt)) == NULL);
		if (ev->sockid == listener) {
			/* if the event is for the listener, accept connection */
			ret = AcceptConnection(ctx, listener);
			if (ret < 0)
				TRACE_EXCP("Accept fails at socket %d\n",listener);
		} else if (ev->events & Q_EPOLLERR) {
			/* error on the connection */
			TRACE_EXCP("[CPU %d] Error on socket %d\n", core, ev->sockid);
		}else if (ev->events & Q_EPOLLIN) {        
			/* an read event from established connection */
			ret = HandleReadEvent(ctx, ev->sockid, &ctx->svars[ev->sockid], 
					ev->pri, core);                      
			if (ret == 0) {
            	/* connection closed by remote host */
                CloseConnection(ctx, ev->sockid, &ctx->svars[ev->sockid]);
			} else if (ret < 0) {
                /* if not EAGAIN, it's an error */                                    
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
					CloseConnection(ctx, ev->data.sockid, 
							&ctx->svars[ev->data.sockid]);
                }
			}
		} else if (ev->events & Q_EPOLLOUT) {
			struct server_vars *sv = &ctx->svars[ev->data.sockid];
			if (sv->rspheader_sent) {
				SendUntilAvailable(ctx, ev->sockid, sv);
			} else {
				TRACE_DEBUG("Socket %d: Response header not sent yet.\n", 
						ev->sockid);
			}
		} else {
			assert(0);
		}
	}
	
	/* destroy mtcp context: this will kill the mtcp thread */
	//mtcp_destroy_context(mctx);
	pthread_exit(NULL);       
        
	return NULL;
}
/*----------------------------------------------------------------------------*/
void
SignalHandler(int signum)
{
	int i;

	for (i = 0; i < core_limit; i++) {
		if (app_thread[i] == pthread_self()) {
			//TRACE_INFO("Server thread %d got SIGINT\n", i);
			done[i] = TRUE;
		} else {
			if (!done[i]) {
				pthread_kill(app_thread[i], signum);
			}
		}
	}
}
/*----------------------------------------------------------------------------*/
struct thread_context *
InitializeServerThread(int core)
{
	struct thread_context *ctx;
	int qid;

	ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
	if (!ctx) {
		TRACE_ERR("Failed to create thread context!\n");
		return NULL;
	}

	ctx->qapp = (qapp_t )calloc(1, sizeof(struct qapp_context));

	/* allocate memory for server variables */
	ctx->svars = (struct server_vars *)
			calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
	if (!ctx->svars) {
		TRACE_ERR("Failed to create server_vars struct!\n");
		return NULL;
	}

	ctx->qapp->app_id = core;

	ctx->qapp->core_id = core + CONFIG.num_stacks;

    qid = qepoll_create(ctx->qapp, MAX_EVENTS / CONFIG.num_servers);
	
	return ctx;
}
/*----------------------------------------------------------------------------*/
static void
show_help(void) 
{
#ifdef USE_OPENSSL
# define TEXT_SSL " (ssl)"
#else
# define TEXT_SSL
#endif
char *b = TEXT_SSL " ("__DATE__ " " __TIME__ ")" \
" - a fast echo webserver\n" \
"usage:\n" \
" -a <#cpus> number of cpu cores that applications will use\n" \
" -s <#cpus> number of cpu cores that qstack will use\n" \
" -p 		 enable the server print " \
" -l 		 set the low-priority task delay " \
" -d 		 set the high-priority task delay " \
" -h         show this help\n" \
"\n"
;
#undef TEXT_SSL
#undef TEXT_IPV6
	write(STDOUT_FILENO, b, strlen(b));
}
/*----------------------------------------------------------------------------*/
static int
get_num(const char *strnum)
{
	return strtol(strnum, (char **) NULL, 10);
}
/*----------------------------------------------------------------------------*/
int 
main(int argc, char **argv)
{
	struct thread_context *ctx[MAX_CPUS];
	int core_stack,core_server,core_print;
	int o, i, qid[10];
	char *host_ip[16] = {0};
	
	num_cores = 23;//GetNumCPUs();
	
	// default settings
	core_stack = MAX_STACK_NUM;
	CONFIG.num_stacks = core_stack;
	core_server = MAX_SERVER_NUM;
	CONFIG.num_servers = core_server;
	core_print = 1;
	task_delay_h = 0;
	task_delay_l = 0;
	
	while(-1 != (o = getopt(argc, argv, "s:a:p:d:l:i:h"))){
		switch(o) {
		case 's':
			core_stack = get_num(optarg);
			CONFIG.num_stacks = core_stack;
			break;
		case 'a':
			core_server = get_num(optarg);
			CONFIG.num_servers = core_server;
			break;
		case 'p':
			core_print = 1;
			break;
		case 'd':
			task_delay_h = get_num(optarg);
			break;
		case 'l':
			task_delay_l = get_num(optarg);
			break;
		case 'i':
			strcpy(host_ip, optarg);
			break;
		case 'h':
		default:
			show_help();
			return EXIT_SUCCESS;
		}
	}
	CONFIG.num_cores = MAX_CORE_NUM;
	
    pthread_t print_states[core_print];
    pthread_attr_t attr;
    cpu_set_t cpus;
    pthread_attr_init(&attr);
	
	qstack_init(core_stack);
	fprintf(stderr, "start host at %s\n", host_ip);
	host_ip_set(host_ip);
	
	/* create mtcp context: this will spawn an mtcp thread */
	q_register_pkt_filter(redis_packet_pri_filter);

	for(i = 0; i < core_server; i++) {
		ctx[i] = InitializeServerThread(i);
	}
		
	int listener = CreateListeningSocket(ctx[0]);
	
	if (listener < 0) {
		TRACE_ERR("Failed to create listening socket.\n");
		exit(-1);
	}

	TRACE_INFO("Qstack initialization finished.\n");

    for (i = 0; i < core_server; i++) {
#ifdef SHARED_NOTHING_MODE
		server_thread[i].core = i;
#else
		server_thread[i].core = i + core_stack;
#endif
		server_thread[i].ctx = ctx[i];
		server_thread[i].listener = listener;
		done[i] = FALSE;
		qstack_create_app(server_thread[i].core, NULL, RunServerThread, 
				(void *)&server_thread[i]);
		app_thread[i] = get_core_context(server_thread[i].core)->rt_ctx->rt_thread;
	}
	
	qstack_join();
	return 0;
}

