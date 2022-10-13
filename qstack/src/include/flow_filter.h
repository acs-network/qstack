/** 
 * @file flow_filter.c
 * @brief add flow filter to NIC,packet will swtich into different nic rx queue by tcp src port
 * @author zhangzhao (zhangzhao@ict.ac.cn) 
 * @date 2022.9.23 
 * @version 1.0
 * @detail Function list: \n
 *	1.flow_filter_init(): use generate_tcp_flow(),create packet switch rule bu tcp src port \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *	1. Date: 2018.8.19 
 *	   Author: zhangzhao 
 *	   Modification: create
  *	2. Date: 2022.9.23 
 *	   Author: zhangzhao 
 *	   Modification: add func note and remove unused func
 */ 
/******************************************************************************/

#ifndef FLOW_FILTER_TEST
#define FLOW_FILTER_TEST
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>
#include <rte_flow.h>

int flow_filter_init(int qstack_num,int port_id);

#endif
