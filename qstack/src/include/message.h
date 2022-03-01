/**
 * @file 	message.h
 * @brief 	high performance message circular queue to deliver print 
 * 			informations, designed for multi-producer-single-consumer
 * @author 	Shen Yifan (shenyifan@ict.ac.cn)
 * @date 	2019.1.14
 * @version 1.0
 * @detail Function list: \n
 *   1. \n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.1.14
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __MESSAGE_H_
#define __MESSAGE_H_
/******************************************************************************/
/* forward declarations */
struct q_message;
typedef struct q_message *msg_t;
typedef struct n21_queue message_queue;
struct q_log;
typedef struct q_log *log_t;
typedef struct circular_queue log_queue;
struct q_fmsg_queue;
typedef struct q_fmsg_queue fmsg_queue;
/******************************************************************************/
/* global macros */
#define USE_FAST_MESSAGE	0
#define MSG_VALIDATE_LENGTH
#define USE_VALIST_ENCODE	0 // use va_list for message encode
#define MSG_BLOCK_SEND		1 // blocking send for message and log
#include <string.h>
#include <stdarg.h>
#include <global_macro.h>
#include "n21_queue.h"
/******************************************************************************/
extern message_queue msgq;
extern log_queue logq;
extern fmsg_queue fmsgq;
extern FILE *fp_log;
extern volatile uint64_t total_log_size;
/*----------------------------------------------------------------------------*/
static void flush_message();
/******************************************************************************/
#ifndef TRACE_EXCP
	#define TRACE_EXCP(f,m...) fprintf(stderr, "[EXCP]" f, ##m);
#endif
#ifndef TRACE_ERR
	#define TRACE_ERR(f,m...) 	do {	\
			fprintf(stderr, "[ERR]@[%10s:%4d] " f,	\
					__FUNCTION__, __LINE__, ##m);	\
			exit(0);	\
		} while(0)
#endif
/******************************************************************************/
/*----------------------------------------------------------------------------*/
#define MAX_MESSAGE_LEN		200
#define MAX_MESSAGE_NUM		800000
#define MAX_MSG_ARG_NUM		12
#define MAX_VALIST_LEN		MAX_MSG_ARG_NUM*8

#define MAX_LOG_BLOCK_LEN	20907000		// 2M=20907152
#define MAX_LOG_BLOCK_NUM	100			
#define LOG_BLOCK_FULL_LEN	20906000
#define MAX_LOG_SIZE		400000000000	// exit after writing 400G log

#define MAX_FMSG_NUM		10000000
/******************************************************************************/
/* data structures */
#define MSG_TYPE_FILELINE	0X80
struct q_message
{
	uint8_t argc;
	uint8_t type;
	uint16_t line;
	char file_name[16];
	char format[MAX_MESSAGE_LEN];
	union {
		uint64_t argv[MAX_MSG_ARG_NUM];
		char valist[MAX_VALIST_LEN];
	};
};

struct q_log
{
	int len;
	char buffer[MAX_LOG_BLOCK_LEN];
};

struct q_fmsg
{
	uint8_t type;
	uint64_t argv[4];
};
struct q_fmsg_queue
{
	struct {
		uint64_t num;
		struct q_fmsg *msg;
	} queue[MAX_CORE_NUM];
};
/******************************************************************************/
/* function declarations */
// fast message queue
static inline void
fmsgq_init()
{
#if USE_FAST_MESSAGE
	int i;
	for (i=0; i<MAX_CORE_NUM; i++) {
		fmsgq.queue[i].num = 0;
		fmsgq.queue[i].msg = (struct q_fmsg*)calloc(MAX_FMSG_NUM, 
				sizeof(struct q_fmsg));
	}
#endif
}
	
static inline void
fmsgq_add(uint8_t core_id, uint8_t type, uint64_t arg0, uint64_t arg1, 
		uint64_t arg2, uint64_t arg3)
{
#if USE_FAST_MESSAGE
	struct q_fmsg *msg = &fmsgq.queue[core_id].msg[fmsgq.queue[core_id].num];
	msg->type = type;
	msg->argv[0] = arg0;
	msg->argv[1] = arg1;
	msg->argv[2] = arg2;
	msg->argv[3] = arg3;
	fmsgq.queue[core_id].num++;
#endif
}

void
fmsgq_flush();
/*----------------------------------------------------------------------------*/
// log process
static inline void
logq_init()
{
	cirq_init(&logq, MAX_LOG_BLOCK_NUM);
	cirq_init_slot(&logq, sizeof(struct q_log));
}

static inline log_t
logq_get_wptr()
{
	log_t ret = (log_t)(*cirq_get_wslot(&logq));
	for (; ret==NULL; ret = (log_t)(*cirq_get_wslot(&logq)));
	return ret;
}

static inline int 
logq_send_log()
{
	int ret = cirq_writen(&logq);
#if MSG_BLOCK_SEND
	for (; ret!=SUCCESS; ret=cirq_writen(&logq));
#else
	if (ret != SUCCESS) {
		TRACE_EXCP("failed to send log\n");
	}
#endif
	return ret;
}

static inline log_t
logq_recv_log()
{
	log_t ret = (log_t)cirq_get(&logq);
	return ret;
}

static inline void 
flush_log(FILE *stream)
{
	log_t log;
	while (log = logq_recv_log()) {
		fwrite(log->buffer, log->len, 1, stream);
		total_log_size += log->len;
		if (total_log_size > MAX_LOG_SIZE) {
			TRACE_ERR("write log file more than 400G!\n");
			exit(0);
		}
	}
}
/*----------------------------------------------------------------------------*/
// message queue 

/**
 * init the message queue
 *
 * @param num_cores		total number of cores
 *
 * @return null
 */
static inline void
msgq_init(int num_cores)
{
#if USE_MESSAGE
	n21q_init(&msgq, num_cores, MAX_MESSAGE_NUM);
	n21q_init_slot(&msgq, sizeof(struct q_message));
	logq_init();
#endif
}

/**
 * get a writable ptr for write in message
 *
 * @param core_id	the id of the current core
 *
 * @return
 * 	return the point to a writable buffer
 *
 * @note
 * 	this function may called on remote core before qstack thread start
 */
static inline msg_t
msgq_get_wptr(int core_id)
{
	if (core_id >= MAX_CORE_NUM) {
		core_id = 0;
	}
	msg_t ret = (msg_t)(*n21q_get_wslot(&msgq, core_id));
	return ret;
}

/**
 * send the written message to priter
 *
 * @param core_id	the core_id of the message sender
 *
 * @return 
 * 	return SUCCESS if success; otherwise return FAILED
 *
 * @note
 * 	this function may called on remote core before qstack thread start
 */
static inline int 
msgq_send_msg(int core_id)
{
	if (core_id >= MAX_CORE_NUM) {
		core_id = 0;
	}
	int ret = n21q_writen(&msgq, core_id);
#if MSG_BLOCK_SEND
	for (; ret!=SUCCESS; ret=n21q_writen(&msgq, core_id));
#else 
	if (ret != SUCCESS) {
		TRACE_EXCP("failed to send message @Core %d\n", core_id);
	}
#endif
	return ret;
}

/**
 * recv the message sent from writer
 *
 * @param q		target message queue
 *
 * @return
 * 	return the message sent from writer
 * @note
 * 	it's not quite safe, the message can be modified if the queue is full
 */
static inline msg_t
msgq_recv_msg()
{
	return (message_queue *)n21q_dequeue_local(&msgq);
}

#if 0
static void
encode_message(msg_t msg, const char *format, ...)
{
	int reg_area_len;
	va_list ap;
	msg->type = 0;
	va_start(ap, format);
	fprintf(stderr, "gp_offset:%u fp_offset:%u reg_save_area:%p overflow:%p\n",
			ap->gp_offset, ap->fp_offset, ap->reg_save_area, 
			ap->overflow_arg_area);
	uint64_t *reg_area = ap->reg_save_area;
	uint64_t *overflow = ap->overflow_arg_area;
	fprintf(stderr, "@msg:%p @format:%p, @ap:%p ap:%p\n", 
			&msg, &format, &ap, ap);
	fprintf(stderr, "reg_area:\n");
	for (i=ap->gp_offset; i<48; i+=8) {
//		fprintf(stderr, "%llx ", reg_area[i/8]);
	}
	fprintf(stderr, "\noverflow_area:\n");
	for (i=0; i<4; i++) {
		fprintf(stderr, "%llx ", overflow[i]);
	}
}
#endif

#if USE_VALIST_ENCODE
#define encode_message(msg, fmt, ...) do {	\
	(msg)->type = 0;	\
	strcpy((msg)->format, (fmt));	\
	(msg)->argc = Q_ARG_COUNT(__VA_ARGS__);	\
	parse_args_va((msg)->valist, (msg)->argc, __VA_ARGS__);	\
} while(0)
#else
/**
 * store arguments in printf format into message struct
 * 
 * @param msg		target message
 * @param fmt		print format
 * @param args...	arguments
 *
 * @return null
 *
 * @note
 * 	
 */
#define encode_message(msg, fmt, ...) do { \
	(msg)->type = 0;	\
	strcpy((msg)->format, (fmt));	\
	(msg)->argc = Q_ARG_COUNT(__VA_ARGS__);	\
	PARSE_ARGS((msg)->argv, __VA_ARGS__);	\
} while(0)

#endif
#if 0
static inline int 
decode_message(char* buffer, msg_t msg)
{
	int ret = 0;
	switch (msg->argc) {
	case 0: 
		strcpy(buffer, msg->format);
		ret = strlen(msg->format);
		break;
	case 1:
	}
}
#endif

static inline int
decode_message(char *buffer, msg_t msg)
{
	va_list ap;
	/* va_args will read from the overflow area if the gp_offset
	 * is greater than or equal to 48 (6 gp registers * 8 bytes/register)
	 * and the fp_offset is greater than or equal to 304 (gp_offset +
	 * 16 fp registers * 16 bytes/register) */
	ap[0].gp_offset = 48;
	ap[0].fp_offset = 304;
	ap[0].overflow_arg_area = (void*)msg->valist;
	ap[0].reg_save_area = NULL;
	int ret = vsprintf(buffer, msg->format, ap);
	return ret;
}

static void
flush_message()
{
#if USE_MESSAGE
	msg_t *msg;
	static log_t cur_log = NULL;
	static int offset = 0; 
	int ret;
	if (!cur_log) {
		cur_log = logq_get_wptr();
	}
	while (msg = msgq_recv_msg()) {
		ret = decode_message(cur_log->buffer+offset, msg);
		offset += ret;
		if (offset > LOG_BLOCK_FULL_LEN) {
			cur_log->len = offset;
			logq_send_log();
			cur_log = logq_get_wptr();
			offset = 0;
//			flush_log(stderr);
		}
	}
#endif
}
/*----------------------------------------------------------------------------*/
#endif //#ifdef __MESSAGE_H_
