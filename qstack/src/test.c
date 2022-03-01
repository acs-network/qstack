 /**
 * @file test.c
 * @brief quick test functions to verify ideas in Qingyun environment
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2021.8.24
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2021.8.24
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#include "qstack.h"
/******************************************************************************/
  // n21_queue test
#define QUEUE_LENGTH	1048576
#define TEST_ROUND		1000000
#define CORE_NUM		4
#define RTE_RING_TEST	0
/*----------------------------------------------------------------------------*/
static struct n21_queue n21qs[CORE_NUM];
static struct rte_ring *ring[CORE_NUM];
static volatile int start_flag = 0;
static uint64_t pro_result[CORE_NUM];
static uint64_t con_result[CORE_NUM];

static void
producer(int id)
{
	uint64_t obj = id;
	int i;
	while (!start_flag);
	uint64_t start_ts = get_time_ns();
	for (i=0; i<TEST_ROUND; i++) {
#if RTE_RING_TEST
		rte_ring_enqueue(ring[i%CORE_NUM], &obj);
#else
		n21q_enqueue(&n21qs[i%CORE_NUM], id, &obj);
#endif
	}
	uint64_t end_ts = get_time_ns();
	pro_result[id] = end_ts - start_ts;
	TRACE_TRACE("producer %d done\n", id);
	sleep(5);
}

static void
consumer(int id)
{
	int i;
	int result = 0;
	while (!start_flag);
	uint64_t start_ts = get_time_ns();
	uint64_t *ret = NULL;
	for (i=0; i<TEST_ROUND; i++) {
#if RTE_RING_TEST
		do {
			rte_ring_dequeue(ring[id], &ret);
		} while (ret == NULL);
#else
		while ((ret=n21q_dequeue(&n21qs[id])) == NULL);
#endif
		result += *ret;
		ret = NULL;
	}
	uint64_t end_ts = get_time_ns();
	con_result[id] = end_ts - start_ts;
	TRACE_TRACE("consumer %d done, result=%llu\n", id, result);
}

// there are #CORE_NUM producers and consumers
static void 
n21_queue_test()
{
	pthread_t pro_thread[CORE_NUM];
	pthread_t con_thread[CORE_NUM];
	int i, j;
	char name[20];

	for (i=0; i<CORE_NUM; i++) {
#if RTE_RING_TEST
		sprintf(name, "testring%d", i);
		ring[i] = rte_ring_create(name, QUEUE_LENGTH, 0, 
				RING_F_SC_DEQ);
#else
		n21q_init(&n21qs[i], CORE_NUM, QUEUE_LENGTH);
#endif
		qps_create_pthread(&pro_thread[i], i, producer, i);
		qps_create_pthread(&con_thread[i], i+CORE_NUM, consumer, i);
	}
	sleep(1);
	start_flag = 1;
	for (i=0; i<CORE_NUM; i++) {
		pthread_join(pro_thread[i], NULL);
		pthread_join(con_thread[i], NULL);
	}
	for (i=0; i<CORE_NUM; i++) {
		fprintf(stdout, "%d. producer:%lluns\t consumer:%lluns\n", 
				i, pro_result[i]/TEST_ROUND, con_result[i]/TEST_ROUND);
	}
}
/******************************************************************************/
// functions called by mainloop
int
test_func1()
{
//	TRACE_TRACE("test for message %d %d %d %d %d %d!\n", 1, 2, 3, 4, 5, 6);

	TRACE_MEMORY("memory cost check: MAX_FLOW_NUM: %d\n"
			"tcp_stream:%lu, num:%lu\n"
			"socket:%lu, num:%lu\n"
			"qevent_buff:%lu, num:%lu\n"
			"qTimeItems_buff:%lu, num:%lu\n"
			, MAX_FLOW_NUM
			, sizeof(struct tcp_stream), MAX_FLOW_NUM
			, sizeof(struct socket), MAX_FLOW_NUM
			, sizeof(struct qepoll_event), QEPOLL_SIZE
			, sizeof(qTimewheelItem_t), MAX_CONCURR
			);
}

int 
test_func2()
{
//	n21_queue_test();
//	exit(0);
	return 1;
}
