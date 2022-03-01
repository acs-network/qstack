#include "flow_filter.h"
#include "qstack.h"
#include "n21_queue.h"
#include "circular_queue.h"
#define SUCCESS  1
#define FAIL -1
#define MAX_FLOW_RULE 2048
#define APP_RETA_SIZE_MAX     (ETH_RSS_RETA_SIZE_512 / RTE_RETA_GROUP_SIZE)
#define MAX_PATTERN_NUM		8

#define SRC_IP ((0<<24) + (0<<16) + (0<<8) + 0) /* src ip = 0.0.0.0 */
#define DEST_IP ((192<<24) + (168<<16) + (1<<8) + 2) /* dest ip = 192.168.1.2 */
#define FULL_MASK 0xffffffff /* full mask */
#define EMPTY_MASK 0x0 /* empty mask */


//struct rte_flow_item_raw *raw_spec[2048];
//struct rte_flow_item_raw *raw_mask[2048];

static struct rte_flow * filter[MAX_FLOW_RULE];
static struct rte_flow_error error[MAX_FLOW_RULE];

static int testmark = 0;

static struct rte_flow_item  eth_item = { RTE_FLOW_ITEM_TYPE_ETH, 0, 0, 0 };
static struct rte_flow_item  end_item = { RTE_FLOW_ITEM_TYPE_END, 0, 0, 0 };

struct rte_flow *
generate_tcp_flow(uint16_t port_id, uint16_t rx_q,
		uint16_t sport, uint16_t src_mask,
		uint16_t dport, uint16_t dest_mask,
		struct rte_flow_error *error)
{
	struct rte_flow_attr attr;
	struct rte_flow_item pattern[MAX_PATTERN_NUM];
	struct rte_flow_action action[MAX_PATTERN_NUM];
	struct rte_flow *flow = NULL;
	struct rte_flow_action_queue queue = { .index = rx_q };
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item_eth eth_mask;
	struct rte_flow_item_vlan vlan_spec;
	struct rte_flow_item_vlan vlan_mask;
	struct rte_flow_item_ipv4 ip_spec;
	struct rte_flow_item_ipv4 ip_mask;

	struct rte_flow_item_tcp tcp_spec;
	struct rte_flow_item_tcp tcp_mask;

        struct rte_flow_item_raw raw_spec;
        struct rte_flow_item_raw raw_mask;

	int res;

	memset(pattern, 0, sizeof(pattern));
	memset(action, 0, sizeof(action));

	/*
	 * set the rule attribute.
	 * in this case only ingress packets will be checked.
	 */
	memset(&attr, 0, sizeof(struct rte_flow_attr));
	attr.ingress = 1;
        attr.priority = 0;
        testmark++;
	/*
	 * create the action sequence.
	 * one action only,  move packet to queue
	 */

	action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
        TRACE_INFO("action[0] address is 0x%p and action[0].type is %d \n",&action[0],action[0].type);
	action[0].conf = &queue;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;

        TRACE_INFO("action[1] address is 0x%p \n",&action[1]);

	/*
	 * set the first level of the pattern (eth).
	 * since in this example we just want to get the
	 * ipv4 we set this level to allow all.
	 */
#if 1

	pattern[0] = eth_item;
#endif
	/*
	 * setting the second level of the pattern (ip).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */
#if 0
	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.dst_addr = htonl(DEST_IP);
	ip_mask.hdr.dst_addr = FULL_MASK;
	ip_spec.hdr.src_addr = htonl(SRC_IP);
	ip_mask.hdr.src_addr = EMPTY_MASK;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = &ip_spec;
	pattern[1].mask = &ip_mask;

	/* the final level must be always type end */
	pattern[2].type = RTE_FLOW_ITEM_TYPE_END;
#else

	memset(&ip_spec, 0, sizeof(struct rte_flow_item_ipv4));
	memset(&ip_mask, 0, sizeof(struct rte_flow_item_ipv4));
	ip_spec.hdr.dst_addr = 0;//htonl(DEST_IP);
	ip_mask.hdr.dst_addr = EMPTY_MASK; //we do not use ip but may use it later
	ip_spec.hdr.src_addr = 0;//htonl(SRC_IP);
	ip_mask.hdr.src_addr = EMPTY_MASK;
	pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
	pattern[1].spec = &ip_spec;
	pattern[1].mask = &ip_mask;
//	pattern[1].last = NULL;
	/*
	 * setting the third level of the pattern (tcp).
	 * in this example this is the level we care about
	 * so we set it according to the parameters.
	 */
#if 1
	memset(&tcp_spec, 0, sizeof(struct rte_flow_item_tcp));
	memset(&tcp_mask, 0, sizeof(struct rte_flow_item_tcp));
	tcp_spec.hdr.dst_port = rte_cpu_to_be_16(dport);
	tcp_mask.hdr.dst_port = rte_cpu_to_be_16(dest_mask);
	tcp_spec.hdr.src_port = rte_cpu_to_be_16(sport);
	tcp_mask.hdr.src_port = rte_cpu_to_be_16(src_mask);

	pattern[2].type = RTE_FLOW_ITEM_TYPE_TCP;
	pattern[2].spec = &tcp_spec;
	pattern[2].mask = &tcp_mask;
//	pattern[2].last = NULL;
	/* the final level must be always type end */
	pattern[3] = end_item;
	pattern[3].type = RTE_FLOW_ITEM_TYPE_END;
#endif
#endif

    TRACE_INFO("raw_spec.pattern is 0x%p and &raw_spec is 0x%p\n",raw_spec.pattern,&raw_spec);
    TRACE_INFO("pattern is 0x%p action address is 0x%p \n",pattern,action);
	res = rte_flow_validate(port_id, &attr, pattern, action, error);
	if (!res)
        {
		flow = rte_flow_create(port_id, &attr, pattern, action, error);
        }
        else
        {
                TRACE_INFO("check pattern fail \n");
                return NULL;
        }
	return flow;
}

int flow_filter_init(int qstack_num,int port_id)
{
    int i = 0;
    int ret = 0;
    int sum = 0;
    for(i = 0; i < 2; i++)    
    {

        filter[i] = generate_tcp_flow(0,i%2,           //uint16_t port_id, uint16_t rx_q
                                      0,0,           //uint16_t sport, uint16_t src_mask,
                                      443 + i ,0xffff,      //uint16_t dport, uint16_t dest_mask,
                                      &error[i]);    //not use can be NULL
        if(filter[i] == NULL)
        {
            printf("the %d rule create fail \n",i);
            return 0;
        }
        
    }

    printf("!!! the %d rule create success \n",i);
    return 1;
}

#if 0 // moved to dpdk_module.c
int rss_filter_init(int qstack_num,int port_id)
{
    uint16_t idx;
    int ret; 
    struct rte_eth_rss_reta_entry64 reta_conf[APP_RETA_SIZE_MAX];
    struct rte_eth_dev_info dev_info;
    memset(reta_conf, 0, sizeof(reta_conf));
    rte_eth_dev_info_get(port_id, &dev_info);
    

    for(idx = 0;idx < dev_info.reta_size; idx++){
       reta_conf[idx / RTE_RETA_GROUP_SIZE].mask = UINT64_MAX;
    }    
    for(idx = 0;idx < dev_info.reta_size; idx++){
		uint32_t reta_id = idx / RTE_RETA_GROUP_SIZE;
		uint32_t reta_pos = idx % RTE_RETA_GROUP_SIZE;
//       if(reta_conf[reta_id].reta[reta_pos]%2 != 0)
//       {
//           reta_conf[reta_id].reta[reta_pos] = reta_conf[reta_id].reta[reta_pos] - 1;
//       }
        reta_conf[reta_id].reta[reta_pos] = idx % qstack_num;
    }    

    ret = rte_eth_dev_rss_reta_update(port_id,reta_conf,dev_info.reta_size);   
    if(ret != 0){
        TRACE_ERROR("RETA update failed\n");
        return FAIL;
    }else{
        TRACE_LOG("RETA update success\n");
        return SUCCESS;
    }    
}
#endif

int flow_director_init(int qstack_num,int port_id)
{



    return SUCCESS;
}


int add_rules()
{
    return 0;
}
