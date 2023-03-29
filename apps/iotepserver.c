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

#include <qstack.h>
#include <api.h>

#include "debug.h"

#ifndef IOTEPSERVER_SETTINGS
	// not iotepserver compile environment
	#define WORKER_PER_SERVER	0
#endif

#define NUM_SERVER_THREAD MAX_STACK_NUM

#define REDIS_DB

#define SOCK

#ifdef REDIS_DB

#include "redis/redis.h"

#define REDIS_PORT 6379
redisContext *redis[NUM_SERVER_THREAD * WORKER_PER_SERVER];
static int do_warmup = 0;

#else

int *redis[NUM_SERVER_THREAD * WORKER_PER_SERVER];

#endif

//#define MAX_FLOW_NUM  (12000000)

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

#define TRACE_LEVEL_UNIVERSAL  TRACELV_DETAIL

#include <sched.h>

//add by songhui
uint64_t requests;
uint64_t responces;

int core_stack,core_server,core_print;

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
    int sid;
    int id;
    int core;    
};
/*----------------------------------------------------------------------------*/
static int num_cores;
static int nb_processors;
static pthread_t app_thread[MAX_CPUS];
static struct sthread_args server_thread[MAX_CPUS];
static int done[MAX_CPUS];
static int driver_pri_offset = 32;
/*----------------------------------------------------------------------------*/
int 
redis_packet_pri_filter(mbuf_t mbuf)
{
	char *payload = mbuf_get_tcp_ptr(mbuf) + driver_pri_offset;
	return (mbuf->pkt_len>80 && payload[5] == 0x01);
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
	qepoll_ctl(ctx->qapp->app_id, sockid, Q_EPOLL_CTL_DEL, GetTickMS(), NULL);
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

#define SUCCESS 1
#define FAILURE 0
#define SLIDE_WIN_SIZE 1024

struct server_stats{
    uint64_t requests_counter;
    uint64_t responses_counter;
    uint64_t update_counter;
    uint64_t loop_time;
    uint64_t req_distri_time;
    uint64_t upd_distri_time;
    uint64_t nevent_num;
    //struct timeval handle_loop_start;
    //struct timeval handle_loop_end;
}w_stats[NUM_SERVER_THREAD];

struct worker_stats {
    struct timeval redis_t_start;
    struct timeval redis_t_end;
    
    uint64_t queue_len;
    uint64_t sum_redis_time;
    uint64_t max_redis_time;
    uint64_t completes; 
    
    uint64_t queue_delay;    
    uint64_t queue_time[SLIDE_WIN_SIZE];  
    uint64_t redis_time[SLIDE_WIN_SIZE];
    uint16_t _size;
    
    uint64_t queue_time_95;
    uint64_t redis_time_95;

    //add by songhui
    uint64_t requests_counter;
    uint64_t responses_counter;
    uint64_t update_counter;

    uint64_t wait_read_events;
    
} g_stats[NUM_SERVER_THREAD * WORKER_PER_SERVER];

struct ForwardPkt
{
    int sock_id;
    int core_id;    
    qmag_t mctx;
    qapp_t qapp;
    struct timeval start_time;
    struct timeval end_time;

	/*@2022.2.23*/ 
	mbuf_t mbuf;
	char payload[6];
};


#define CAP_REDIS_BUFFER (1024*100)
FILE *stats;

struct ForwardPktBuffer
{
    volatile struct ForwardPkt pktQueue[CAP_REDIS_BUFFER];
    int capacity;
    volatile int _size;
    volatile int _head;
    volatile int _tail;
    //pthread_mutex_t mutex;
};

//add by songhui
struct ForwardPktBuffer forwardUpdate[NUM_SERVER_THREAD * WORKER_PER_SERVER];

struct ForwardPktBuffer forwardQuery[NUM_SERVER_THREAD * WORKER_PER_SERVER];

#ifdef REDIS_DB
static int 
queryDatabase(const char *key, const char *cmpValue, redisContext *cluster, const int database, int core_id)
{
    char value[VALUE_LEN];
    int error;

    error = get(cluster, key, value, database, core_id);

    if (error == 0) {
        
        if (strcmp(value, "nil") == 0) 
        {
             set(cluster, key, cmpValue, database, core_id);
        }
        return SUCCESS;
    } else {
        return FAILURE;
    }
}
#endif

/*@2022.2.23*/
#ifdef REDIS_DB
static int 
simpQuerytest(const char *key, const char *cmpValue, redisContext *cluster, const int database, int core_id)
{
    char value[VALUE_LEN];
    int error;

    error = get(cluster, key, value, 1, core_id);
    if (error == 0) {
        if (strcmp(value, "nil") == 0) 
        {
        set(cluster, key, cmpValue, 1, core_id);
        }
		else {
			value[0]=value[0]+1;
			set(cluster, key, value, 1, core_id);
		}
		get(cluster, key, value, 2, core_id);
        return SUCCESS;
    } 
	else {
        return FAILURE;
    }
}
#endif

void 
initForwardBuffer(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{
            
    int j;
    for (j = 0; j < CAP_REDIS_BUFFER; j ++) {
            forwardBuffer[buffer_id].pktQueue[j].sock_id = 0;
            forwardBuffer[buffer_id].pktQueue[j].core_id = 0;
    }
        
    forwardBuffer[buffer_id]._head = forwardBuffer[buffer_id]._tail = forwardBuffer[buffer_id]._size = 0;
    /*pthread_mutex_init(&forwardBuffer[buffer_id].mutex, NULL);*/
}

struct ForwardPkt* 
head(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{
    return forwardBuffer[buffer_id].pktQueue + (forwardBuffer[buffer_id]._head % CAP_REDIS_BUFFER); 
}

void 
headNext(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{ 
    forwardBuffer[buffer_id]._head = (forwardBuffer[buffer_id]._head + 1) % CAP_REDIS_BUFFER; 
}

struct ForwardPkt* 
tail(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{
    return forwardBuffer[buffer_id].pktQueue + (forwardBuffer[buffer_id]._tail % CAP_REDIS_BUFFER); 
}

void 
tailNext(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{
    forwardBuffer[buffer_id]._tail = (forwardBuffer[buffer_id]._tail + 1) % CAP_REDIS_BUFFER; 
}

u_int 
forwardBufferSize(struct ForwardPktBuffer *forwardBuffer,int buffer_id) 
{
#ifdef STATISTIC_FORWARD_BUFF_LEN
	int ret = (forwardBuffer[buffer_id]._tail + CAP_REDIS_BUFFER - 
			forwardBuffer[buffer_id]._head) % CAP_REDIS_BUFFER;
	#ifdef IOTEPSERVER_SETTINGS
	DSTAT_CHECK_SET(ret > get_global_ctx()->forward_buff_len[buffer_id], 
			get_global_ctx()->forward_buff_len[buffer_id], ret);
	#endif
	return ret;
#else
	return (forwardBuffer[buffer_id]._head != forwardBuffer[buffer_id]._tail);
#endif
}

u_int forwardBufferFull(struct ForwardPktBuffer *forwardBuffer,int buffer_id) {
    return ((forwardBuffer[buffer_id]._tail + 1) % CAP_REDIS_BUFFER == forwardBuffer[buffer_id]._head);
}

struct worker_stats total = {0};

void 
PrintStats() 
{
	unsigned long long now,prev = 0;
    	int i, j;
	while(1) {    
		print_network_state();
	}
}

void 
process_requests(void *cluster,struct app_buffer *abuff)
{	
    qmag_t mctx;
    int sockid;
    int core_id;
    int ret = 0;
	int len;
    struct timeval start_time;
    char *response;
	mbuf_t mbuf;
	
    int id = abuff->id;
    struct ForwardPkt *req = head(forwardQuery,id); 
    qapp_thread[id].core_id = abuff->core;
    sockid = req->sock_id;
    core_id = req->core_id;
    //start_time = req->start_time;
    headNext(forwardQuery,id);            

    rs_ts_add(rs_ts_get_from_sock(sockid), REQ_ST_REDIS);
				
#ifdef REDIS_DB
    char buffer[33]; 
    sprintf(buffer, "%d", sockid);        
	/*@2022.2.23*/     
	simpQuerytest(buffer,req->payload , cluster, 1, id); 
#endif
    rs_ts_add(rs_ts_get_from_sock(sockid), REQ_ST_WORKER);

    //if (core_id) {
    mbuf = q_get_wmbuf(qapp_thread + id, &response, &len);
    sprintf(response, "HTTP/1.1 %d %s\r\n"
                "Date: %s\r\n"
                "Server: Webserver on Middlebox TCP (Ubuntu)\r\n"
                "Content-Length: %ld\r\n"
                "Connection: %s\r\n\r\n", 
                200, StatusCodeToString(200), "25.11.1985", 100, "keepalive");
	response[6] = 0x3;
	
	mbuf_print_detail(mbuf);


	ret = q_send(qapp_thread + id, sockid, mbuf, 146, 0);
	if  (ret != 146){
	    TRACE_EXCP("failed q_send() @ Socket %u, ret:%d, errno: %d\n", 
				sockid, ret, errno);
	}else{	
	    g_stats[id].responses_counter += 1;
 	}
    //}

   g_stats[id].queue_len += forwardQuery[id]._size;
}

void 
process_updates(void *cluster,struct app_buffer *abuff)
{	
    qmag_t mctx;
    int sockid;
    int core_id;
    int ret = 0;
    struct timeval start_time;
    char response[HTTP_HEADER_LEN];	

    int id = abuff->id;	
    struct ForwardPkt *req = head(forwardUpdate,id);
    sockid = req->sock_id;
    core_id = req->core_id;
    //start_time = req->start_time;
    headNext(forwardUpdate,id);

#ifdef REDIS_DB
    char buffer[33];
    sprintf(buffer, "%d", sockid);
    //queryDatabase(buffer, "2", cluster, 1, id);
    //queryDatabase(buffer, "4", cluster, 2, id);
	/*@2022.2.23*/ 
	//req->payload[6]="\0";
	simpQuerytest(buffer,req->payload , cluster, 1, id); 
#endif

}

static void
redis_warkup(int id, int sid)
{
#ifdef REDIS_DB
	int sockid;
	int i;
    char buffer[33]; 

	if (!do_warmup) {
		return;
	}
	for (sockid=sid*MAX_SOCKET_PSTACK; sockid<(sid+1)*MAX_SOCKET_PSTACK; 
			sockid++) {
	   	sprintf(buffer, "%d", sockid);        
	   	queryDatabase(buffer, "2", redis[id], 1, id);            
	   	queryDatabase(buffer, "4", redis[id], 2, id);
	}
	TRACE_TRACE("finish redis-warup of Socket %d-%d on worker %d and Server %d\n", 
			sid*MAX_SOCKET_PSTACK, (sid+1)*MAX_SOCKET_PSTACK-1, id, sid);
#endif
}

void* 
redis_requests(void* args) 
{
        
    struct app_buffer abuff = *(struct app_buffer *)args;//sched_getcpu() % NUM_REDIS_BUFFER;
    
    struct timeval start_time;
    
    int i = abuff.id;
    int sid = abuff.sid;
//	TRACE_TRACE("Server ID: %lu, Buffer ID: %lu\n", 
//			  sid, i);
    
#ifdef REDIS_DB
	#ifdef SOCK
	int sock = REDIS_PORT + i;
	int port = -1;
	#else
	int sock = -1;
    int port = REDIS_PORT + i;
	#endif
    redis[i] = connectRedis(port, sock);    
    
    TRACE_TRACE("Server ID: %lu, Buffer ID: %lu, Redis port:%d sock:%d, Redis:%p\n", 
			sid, i, port, sock, redis[i]);
	printf("Server ID: %lu, Buffer ID: %lu, Redis port:%d sock:%d, Redis:%p\n", 
			  sid, i, port, sock, redis[i]);
	redis_warkup(i, sid);
#else
    redis[i] = NULL;
#endif

    srand((unsigned)time(NULL));
    
    initForwardBuffer(forwardQuery,i);
    initForwardBuffer(forwardUpdate,i);       
 
    memset(&g_stats[i], 0, sizeof(struct server_stats));
 
    //fprintf(stderr, "ID: %lu, CPU:%d, Buffer: %d\n", pthread_self(), sched_getcpu(), i);
                 
    while(TRUE)
	{
    	//gettimeofday(&g_stats[i].redis_t_start, NULL); 
        while(forwardBufferSize(forwardQuery,i)) {                      
			process_requests(redis[i],&abuff);
        }
		while(forwardBufferSize(forwardUpdate,i)) {
			//single_ts_start(abuff.core);
            process_updates(redis[i],&abuff);
			//single_ts_end(abuff.core, 1);
			while(forwardBufferSize(forwardQuery,i)) {
				process_requests(redis[i],&abuff);
			}
        }
	}

	    

	/*gettimeofday(&g_stats[i].redis_t_end, NULL);
                             
        uint64_t tdiff = (g_stats[i].redis_t_end.tv_sec - g_stats[i].redis_t_start.tv_sec) * 
					1000000 + (g_stats[i].redis_t_end.tv_usec - g_stats[i].redis_t_start.tv_usec);

        g_stats[i].sum_redis_time += tdiff;
        if (tdiff > g_stats[i].max_redis_time)
        	g_stats[i].max_redis_time = tdiff;
            
        uint64_t delay = (g_stats[i].redis_t_end.tv_sec - start_time.tv_sec) * 1000000 +
                    (g_stats[i].redis_t_end.tv_usec - start_time.tv_usec);
        g_stats[i].queue_delay += delay;
        g_stats[i].completes ++;     
            
        if (g_stats[i]._size < SLIDE_WIN_SIZE)
		{                
            g_stats[i].queue_time[g_stats[i]._size] = delay;
            g_stats[i].redis_time[g_stats[i]._size] = tdiff;
            g_stats[i]._size ++;
        }*/
 
#ifdef REDIS_DB
    disconnectDatabase(redis[i]);
#endif
}

int pos_query[MAX_CPUS] = {0};
int pos_update[MAX_CPUS] = {0};

//#define PIPE_LINE
/*----------------------------------------------------------------------------*/
static int 
HandleReadEvent(struct thread_context *ctx, int sockid, struct server_vars *sv, int pri, int core_id)
{
	struct qepoll_event ev;
	char *buf;
	time_t t_now;
	char t_str[128];
	mbuf_t mbuf;
	int i, index, id;
	uint32_t len; 

	struct timespec req_start,req_end;
	struct timespec upd_start,upd_end;
    
	id = ctx->qapp->app_id;
	/* HTTP request handling */
	mbuf = q_recv(ctx->qapp, sockid, &buf, &len, 0);
	if (!len) {
		q_close(ctx->qapp, sockid);
	}
	if (!mbuf) {
        //fprintf(stderr, "error reading\n");
	    return len;
	}

	mbuf_print_detail(mbuf);
	// TODO: multi-core mode
        
	/*memcpy(sv->request + sv->recv_len, (char *)buf, MIN(len, HTTP_HEADER_LEN - sv->recv_len));	
             
        sv->recv_len += len;
	
        sv->keep_alive = TRUE;*/
      
	TRACE_DEBUG("Socket %d File size: %ld (%ldMB)\n", 
			sockid, sv->fsize, sv->fsize / 1024 / 1024);
 
	/* Response header handling */
	/*time(&t_now);
	strftime(t_str, 128, "%a, %d %b %Y %X GMT", gmtime(&t_now));*/   
	
 
#ifndef PIPE_LINE
	if (pri){
//		clock_gettime(CLOCK_MONOTONIC, &req_start);
		i = pos_query[id];
		if ( i == 0 || i >= WORKER_PER_SERVER)
			i = 0; 
		index = i + id * WORKER_PER_SERVER;
		while(forwardBufferFull(forwardQuery,index)){
			i = (i + 1) % WORKER_PER_SERVER;
			index = i + id * WORKER_PER_SERVER;
        }   
		pos_query[id] = i;
        struct ForwardPkt *req = tail(forwardQuery,index);
        req->sock_id = sockid;
        req->core_id = core_id;
		/*@2022.2.23*/ 
		req->mbuf=mbuf;
		strncpy(req->payload,buf,6);
        tailNext(forwardQuery,index); 
//      gettimeofday(&req->start_time, NULL);
//		clock_gettime(CLOCK_MONOTONIC, &req_end);
//		w_stats[id].req_distri_time = (req_end.tv_sec - req_start.tv_sec) * 1000000000 
//				+ (req_end.tv_nsec - req_start.tv_nsec);
		rs_ts_add(rs_ts_get_from_sock(sockid), REQ_ST_DISTRI);
		w_stats[id].requests_counter += 1;
		g_stats[index].requests_counter += 1;
		pos_query[id]++;       
        if (pos_query[id] == WORKER_PER_SERVER)
			pos_query[id] = 0; //core_id*(WORKER_PER_SERVER/core_limit); 
	}else{
//		clock_gettime(CLOCK_MONOTONIC, &upd_start);
		i = pos_update[id];
		if ( i == 0 || i >= WORKER_PER_SERVER)
			i = 0; 
		index = i + id * WORKER_PER_SERVER;
		while(forwardBufferFull(forwardUpdate,index)){
			i = (i + 1) % WORKER_PER_SERVER;
			index = i + id * WORKER_PER_SERVER;
        }   
		pos_update[id] = i;
        struct ForwardPkt *req = tail(forwardUpdate,index);
        req->sock_id = sockid;
		req->core_id = 0;
		req->mctx = ctx->mctx;   
		req->mbuf=mbuf;
		//req->payload=buf;
		strncpy(req->payload,buf,6);

//      gettimeofday(&req->start_time, NULL);
        tailNext(forwardUpdate,index);
//		clock_gettime(CLOCK_MONOTONIC, &upd_end);
/*    		w_stats[id].upd_distri_time = (upd_end.tv_sec - upd_start.tv_sec) * 1000000000 
					+ (upd_end.tv_nsec - upd_start.tv_nsec);
		w_stats[id].update_counter += 1;*/	
		g_stats[index].update_counter += 1;	
		pos_update[id]++;       
        if (pos_update[id] == WORKER_PER_SERVER)
			pos_update[id] = 0; //core_id*(WORKER_PER_SERVER/core_limit); 
	} 
	
	/*@2022.2.23*/ 
	q_free_mbuf(ctx->qapp->core_id, mbuf);      		     
#else
                
	//gettimeofday(&g_stats[i].redis_t_start, NULL); 
        
#ifdef REDIS_DB
    char buffer[33];        
    sprintf(buffer, "%d", sockid);        
    //queryDatabase(buffer, "2", cluster, 1, i);
    //queryDatabase(buffer, "4", cluster, 2, i);        
#endif

        
    /*gettimeofday(&g_stats[i].redis_t_end, NULL);

 	uint64_t tdiff = (g_stats[i].redis_t_end.tv_sec - g_stats[i].redis_t_start.tv_sec) * 1000000 +
                    (g_stats[i].redis_t_end.tv_usec - g_stats[i].redis_t_start.tv_usec);

    g_stats[i].sum_redis_time += tdiff;
    if (tdiff > g_stats[i].max_redis_time)
    	g_stats[i].max_redis_time = tdiff;
    g_stats[i].completes ++;*/     
        
        
    /*if ((rand() % 100) < 5) {
    	sprintf(response, "HTTP/1.1 %d %s\r\n"
                            "Date: %s\r\n"
                            "Server: Webserver on Middlebox TCP (Ubuntu)\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: %s\r\n\r\n", 
                            scode, StatusCodeToString(scode), t_str, sv->fsize, keepalive_str);

        len = 146;
        TRACE_APP("Socket %d HTTP Response: \n%s", sockid, response);

        sent = mtcp_write(ctx->mctx, sockid, response, len);
        TRACE_APP("Socket %d Sent response header: try: %d, sent: %d\n", 
                         sockid, len, sent);
        sv->rspheader_sent = TRUE;
        
    }*/

#endif        

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
	ret = q_listen(ctx->qapp, listener, 100000);
	if (ret < 0) {
		TRACE_ERR("mtcp_listen() failed!\n");
		return -1;
	}
	
	/* wait for incoming accept events */
	/*ev = CreateQevent(0, ctx->core, listener);
	ev->events = Q_EPOLLIN;
	qepoll_ctl(ctx->core, Q_EPOLL_CTL_ADD, GetTickMS(), ev);*/

	return listener;
}
/*----------------------------------------------------------------------------*/
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

		sv = &ctx->svars[c];
		CleanServerVariable(sv);
		TRACE_DEBUG("New connection %d accepted.\n", c);
		//ev = CreateQevent(0, ctx->core, c);
		ev.events = Q_EPOLLIN;
		//now = GetTickMS();
		qepoll_ctl(ctx->qapp->app_id, c, Q_EPOLL_CTL_ADD, -1, &ev);
		TRACE_DEBUG("Socket %d registered.\n", c);

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
	int id = server_thread.id;
	struct thread_context *ctx = server_thread.ctx;
	int listener;
	struct qepoll_event *events, *ev;
	int nevents;
	int i, ret;
	int do_accept;
	unsigned long long now, prev;

	ctx->qapp->app_id  = id;
	ctx->qapp->core_id = core;

	struct timeval loop_start,loop_end;
              
	if (!ctx) {
		TRACE_ERR("Failed to initialize server thread.\n");
		return NULL;
	}

	events = q_alloc_epres(MAX_EVENTS);
	if (!events) {
		TRACE_ERR("Failed to create event struct!\n");
		exit(-1);
	}

	/*listener = CreateListeningSocket(ctx);
	if (listener < 0) {
		TRACE_ERR("Failed to create listening socket.\n");
		exit(-1);
	}*/
	listener = server_thread.listener;

	struct event_mgt evmgt;
    eventmgt_init(&evmgt, MAX_HIGH_EVENTS, MAX_LOW_EVENTS);
	
	while (!done[core]) {
		while ((ev = eventmgt_get(id, &evmgt)) == NULL);
//		gettimeofday(&loop_start, NULL);
                if (ev->sockid == listener) {
                        /* if the event is for the listener, accept connection */
                        ret = AcceptConnection(ctx, listener);
                        if (ret < 0)
                                TRACE_EXCP("Accept fails at socket %d\n",listener);
                } else if (ev->events & Q_EPOLLERR) {

                        /* error on the connection */
                        TRACE_EXCP("[CPU %d] Error on socket %d\n", core, ev->sockid);
                }else if (ev->events & Q_EPOLLIN) {
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

//		gettimeofday(&loop_end, NULL);

/*		w_stats[id].loop_time = (loop_end.tv_sec - loop_start.tv_sec) * 1000000 
					+ (loop_end.tv_usec - loop_start.tv_usec); */
	}
 
	return NULL;
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
			calloc(MAX_FLOW_NUM / core_server, sizeof(struct server_vars));
	if (!ctx->svars) {
		TRACE_ERR("Failed to create server_vars struct!\n");
		return NULL;
	}

	ctx->core = core;

	ctx->qapp->app_id = core;
	
	ctx->qapp->core_id = core + core_stack;

	qid = qepoll_create(ctx->qapp, MAX_EVENTS / core_server);
	
	return ctx;
}
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
" -w 		 start with redis warmup " \
" -i 		 set host ip address " \
" -f 		 set driver_priority flag offset to TCP header(32 as default) " \
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
	qapp_t dispatcher;
	int o, i, qid[10];
	char *host_ip[16] = {0};
	strcpy(host_ip, "192.168.86.100");
	
	num_cores = 23;//GetNumCPUs();
	
	core_stack = MAX_STACK_NUM;
	CONFIG.num_stacks = core_stack;
	core_server = MAX_STACK_NUM;
	core_print = 1;
	
	while(-1 != (o = getopt(argc, argv, "s:a:p:w:i:f:h"))){
		switch(o) {
		case 's':
			core_stack = get_num(optarg);
			CONFIG.num_stacks = core_stack;
			break;
		case 'a':
			core_server = get_num(optarg);
			break;
		case 'p':
			core_print = 1;
			break;
		case 'w':
			do_warmup = 1;
			break;
		case 'i':
			strcpy(host_ip, optarg);
			break;
		case 'f':
			driver_pri_offset = get_num(optarg);
			break;
		case 'h':
		default:
			show_help();
			return EXIT_SUCCESS;
		}
	}
    qconfig_t qcfg;
	
    pthread_t process_requests[nb_processors],print_states[core_print];
    pthread_attr_t attr;
    cpu_set_t cpus;
    pthread_attr_init(&attr);
    
    nb_processors = WORKER_PER_SERVER * core_server;
    struct app_buffer buffer[nb_processors];
	
    CONFIG.num_servers = core_server;

    CONFIG.num_apps = core_server + nb_processors;
    
    CONFIG.num_cores = core_stack + core_server + nb_processors;
	num_cores = CONFIG.num_cores;

    qstack_init(core_stack);
	fprintf(stderr, "start host at %s\n", host_ip);
	host_ip_set(host_ip);
	q_register_pkt_filter(redis_packet_pri_filter);
		
	/* create mtcp context: this will spawn an mtcp thread */

//	qcfg = get_qconf();
	
//	core_stack = qcfg->num_stack;
//	core_server = qcfg->num_app;
	
    qapp_thread = (qapp_t)calloc(nb_processors, sizeof(struct qapp_context));

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
		server_thread[i].core = i + core_stack;
		server_thread[i].ctx = ctx[i];
		server_thread[i].id = i;
		server_thread[i].listener = listener;
		done[i] = FALSE;
		qstack_create_app(server_thread[i].core, &app_thread[i],
				RunServerThread, (void *)&server_thread[i]); 
	}
        
	for (i = 0; i < nb_processors; i ++) {
        	buffer[i].id = i;
			buffer[i].sid = i / WORKER_PER_SERVER;
			buffer[i].core = core_stack + core_server + i;
        	qstack_create_app(buffer[i].core, &process_requests[i], redis_requests, (void *) &buffer[i]);
    	}
	
	qstack_join();

	return 0;
}

