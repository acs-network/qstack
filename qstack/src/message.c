 /**
 * @file message.c
 * @brief 	high performance message circular queue to deliver print 
 * 			informations, designed for multi-producer-single-consumer
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.11.5
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.11.5
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
/*----------------------------------------------------------------------------*/
#include "message.h"
#include "string.h"
/******************************************************************************/
FILE *fp_log;
message_queue msgq;
log_queue logq;
fmsg_queue fmsgq = {0};
volatile uint64_t total_log_size = 0;
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
fmsgq_flush()
{
#if USE_FAST_MESSAGE
	char fmt[10][200];
	strcpy(fmt[0], "[0]: %llu, %llu %llu %llu\n");
	int i, j;
	struct q_fmsg *msg;
	for (i=0; i<MAX_CORE_NUM; i++) {
		fprintf(fp_log, "----------------------------------------\n"
				"fast_message from core %d\n", i);
		for (j=0; j<fmsgq.queue[i].num; j++) {
			msg = &fmsgq.queue[i].msg[j];
			fprintf(fp_log, fmt[msg->type],
					msg->argv[0], msg->argv[1], msg->argv[2], msg->argv[3]);
		}
	}
#endif
}
/******************************************************************************/
/*----------------------------------------------------------------------------*/
