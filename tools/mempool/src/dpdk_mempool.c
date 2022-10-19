/*
  * dpdk_mempool
  * An implementation of interface between qingyun stack and hugepage mempool
  * qingyun TCP/IP stack can alloc/release chunk space by using func in this module
  *
  * Based on dpdk code.
  *
  * Authors: zhangzhao
  *
*/
 /**
 * @file 
 * @brief 
 * @author 
 * @date 
 * @version 
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.08.30
 *       Author: zhangzhao
 *       Modification: create
 *   2. Date:
 *       Author:
 *       Modification:
 */
/******************************************************************************/
/* make sure to define trace_level before include! */

#ifndef TRACE_LEVEL
    #define TRACE_LEVEL    TRACELV_DEBUG
#endif
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>
#include <assert.h>
#include <stdlib.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_atomic.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>

#include "./include/dpdk_chunk.h"
#include "./include/dpdk_mempool.h"
/******************************************************************************/
/* local macros */
#define CACHE_FLUSHTHRESH_MULTIPLIER 1.5
#define CALC_CACHE_FLUSHTHRESH(c)    \
    ((typeof(c))((c) * CACHE_FLUSHTHRESH_MULTIPLIER))
/******************************************************************************/
/* global variables */
struct Mempool_t mempool_t[MAX_TYPES];
/******************************************************************************/
/* forward declarations */
/******************************************************************************/
/* local data structures */
/******************************************************************************/
/* local static functions */


const struct rte_memzone * rte_memzone_get(const char *name, size_t len, int socket_id,unsigned flags){
    const struct rte_memzone *mz = rte_memzone_lookup(name);
    if (mz == NULL){
        mz = rte_memzone_reserve(name, len, socket_id, flags);
    }
    return mz;
}

static struct rte_mempool *rte_private_pool_create(const char *name, unsigned n,unsigned cache_size, uint16_t priv_size, int core_id,int type){    
    int chunk_size = priv_size;
    int chunk_num = n;
    struct rte_mempool * mp = NULL;
    const struct rte_memzone *mz ;
    struct rte_mempool_cache *cache = NULL;
    int lcore_id;
    //first search for target mz     
    mz = rte_memzone_lookup(name);
    if (mz == NULL){
    //    TRACE_INFO("in rte_pool_create we can not find target memzone\n");
        return NULL;
    }
    //TRACE_INFO("find target mz space in rte_pool_create\n");
    //second create mempool space
    struct rte_mempool_list *mempool_list;
    struct rte_tailq_entry *te = NULL;
    size_t mempool_size;
    unsigned int mz_flags = RTE_MEMZONE_1GB|RTE_MEMZONE_SIZE_HINT_ONLY;
    struct rte_mempool_objsz objsz;
    int ret;
    unsigned flags = 0;
    //TRACE_INFO("/* compilation-time checks */ \n");
    /* compilation-time checks */
    RTE_BUILD_BUG_ON((sizeof(struct rte_mempool) &
              RTE_CACHE_LINE_MASK) != 0);
    RTE_BUILD_BUG_ON((sizeof(struct rte_mempool_cache) &
              RTE_CACHE_LINE_MASK) != 0);

    /* asked cache too big */
    if (cache_size > RTE_MEMPOOL_CACHE_MAX_SIZE ||
        CALC_CACHE_FLUSHTHRESH(cache_size) > n) {
        rte_errno = EINVAL;
        return NULL;
    }
    /* "no cache align" imply "no spread" */
    if (flags & MEMPOOL_F_NO_CACHE_ALIGN)
        flags |= MEMPOOL_F_NO_SPREAD;

    /* calculate mempool object sizes. */
    if (!rte_mempool_calc_obj_size(priv_size, flags, &objsz)) {
        rte_errno = EINVAL;
        return NULL;
    }
    //TRACE_INFO("begin to set mp and mz->addr is 0x%x \n",mz->addr);
    mp = mz->addr;
    //TRACE_INFO("(sizeof(struct rte_mempool_cache) * RTE_MAX_LCORE) is %d and RTE_MAX_LCORE is %d \n",sizeof(struct rte_mempool_cache),RTE_MAX_LCORE);
    //TRACE_INFO("MEMPOOL_HEADER SIZE is %d and mz->len is %d \n",MEMPOOL_HEADER_SIZE(mp, cache_size),mz->len);
    if(MEMPOOL_HEADER_SIZE(mp, cache_size) > mz->len){
        //TRACE_INFO("mz is less zhan cache request \n");
        memset(mp, 0, MEMPOOL_HEADER_SIZE(mp, cache_size));
    }else{

        //TRACE_INFO("mz is bigger zhan cache request \n");
        memset(mp, 0, mz->len);
    }
    //TRACE_INFO("init mp success \n");

//    snprintf(mp->name, RTE_MEMZONE_NAMESIZE, "MEMZONE_INFO_%d_TYPE_%d", i,type);
    ret = snprintf(mp->name, sizeof(mp->name), "MEMZONE_INFO_%d_TYPE_%d", core_id,type);
    if (ret < 0 || ret >= (int)sizeof(mp->name)) {
        rte_errno = ENAMETOOLONG;
        return NULL;
    }
    rte_wmb();
    mp->mz = mz;
    mp->size = n;
    mp->flags = flags;
    mp->socket_id = mz->socket_id;
    mp->elt_size = objsz.elt_size;
    mp->header_size = objsz.header_size;
    mp->trailer_size = objsz.trailer_size;
    /* Size of default caches, zero means disabled. */
    mp->cache_size = cache_size;
    mp->private_data_size = 0;//private_data_size;
    STAILQ_INIT(&mp->elt_list);//this is for 
    STAILQ_INIT(&mp->mem_list);

    /*
 *      * local_cache pointer is set even if cache_size is zero.
 *           * The local_cache points to just past the elt_pa[] array.
 *                */
    mp->local_cache = (struct rte_mempool_cache *)
        RTE_PTR_ADD(mp, MEMPOOL_HEADER_SIZE(mp, 0));

    /* Init all default caches. */
    if (cache_size != 0) {
        for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++){
            cache = &mp->local_cache[lcore_id];
            cache->size = cache_size;
            cache->flushthresh = CALC_CACHE_FLUSHTHRESH(cache_size);
            cache->len = 0;
        }
    }

    rte_rwlock_write_unlock(RTE_EAL_MEMPOOL_RWLOCK);

    return mp;
}

static struct rte_mempool *rte_pool_create(const char *name, unsigned n,unsigned cache_size, uint16_t priv_size, int core_id,struct Mempool_t * targ){    
    struct rte_mempool *mp;
    struct rte_pktmbuf_pool_private mbp_priv;//only used for type 0 rte_mbuf
    int type = -1;
    int max_core = 0;
    const char *mp_ops_name = name;
    unsigned elt_size;
    unsigned data_root_size;//this is for mbuf payload,for other chunk this is 0
    int numa_id;
    unsigned flags = 0;
    if(core_id != -1){
        numa_id = rte_lcore_to_socket_id(core_id);
        flags = (MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET);
    }else{
        numa_id = SOCKET_ID_ANY;
        flags = 0;
    }
    type = targ->type;
    max_core = targ->max_core;

    int ret;
    switch (type){
        case 0://this is rte_mbuf pool
        //    TRACE_MEMORY("this is rte_mbuf pool\n");
#if 0
            priv_size = 0;  //this is rte_mbuf pool,priv_size use defalt 0
            elt_size = sizeof(struct rte_mbuf) + (unsigned)priv_size + (unsigned)PKT_MBUF_DATA_SIZE;
            cache_size = 0; //this is rte_mbuf pool,cache_size use defalt 32
            mbp_priv.mbuf_data_room_size = PKT_MBUF_DATA_SIZE;
            mbp_priv.mbuf_priv_size = priv_size;
            mp = rte_mempool_create_empty(name, n, elt_size, cache_size,sizeof(struct rte_pktmbuf_pool_private), numa_id, flags);        
#endif
            //with cache
            //mp = rte_pktmbuf_pool_create(name,n,32,0,RTE_MBUF_DEFAULT_BUF_SIZE,numa_id);
            //without cache 
            mp = rte_pktmbuf_pool_create(name, n, 32, 0, RTE_MBUF_DEFAULT_BUF_SIZE, numa_id);
            if(mp == NULL){
                TRACE_EXCP("rte_mbuf pool is NULL !!!\n");
                return NULL;
            }
            //rte_pktmbuf_pool_init(mp, &mbp_priv);
        break;
        case 1://this is data_type_1 pool    
                    mp = rte_mempool_create(name, n, priv_size, 32, 0, NULL, NULL, targ->chunk_init, type, numa_id, flags);
            break;

        case 2://this is chunk_head pool
        //    TRACE_MEMORY("this is type 2 pool\n");
        //    TRACE_MEMORY("name is %s n is %d priv_size is %d targ->chunk_init is 0x%x,type is %d \n",name,n,priv_size,targ->chunk_init,type);
            mp = rte_mempool_create(name, n, priv_size, 0, 0, NULL, NULL, targ->chunk_init, type, numa_id, flags);
            if(mp != NULL){
            //    TRACE_MEMORY("in this test mp->ops_index is %d \n",mp->ops_index);
            }else{
                TRACE_EXCP("return NULL can not get mp \n");
            }
                break;
        case 3:    //this is data_type_3 pool

                mp = rte_mempool_create(name, n, priv_size, 32, 0, NULL, NULL, targ->chunk_init, type,numa_id, flags);
                break;
        case 4://this is data_type_4 pool
            
                mp = rte_mempool_create(name, n, priv_size, 32, 0, NULL, NULL, targ->chunk_init, type, numa_id, flags);
        break;
        default:
            return NULL;
    }
    return mp;

}

static struct rte_mempool *rte_chunk_pool_create(const char *name, unsigned n, unsigned cache_size, uint16_t priv_size, int core_id, struct Mempool_t *targ){    
    //TRACE_INFO("begin to run rte_chunk_pool_create\n");
    struct rte_mempool *mp = NULL;
    mp =  rte_pool_create(name, n, cache_size,  priv_size, core_id, targ);
    if(mp == NULL){
        TRACE_EXCP("get mp from rte_pool_create functions fail n is %d priv_size is %d\n",n,priv_size);
        return NULL;
    }else{
       // TRACE_INFO("we get a new mp and mp's addr is 0x%x \n", mp);
    }
    return mp;
    
}


static  int rte_mempool_freeback(const struct rte_mempool *mp,int type){

    if(type < 0 || type >= MAX_TYPES){
        TRACE_INFO("type wrong and free mempool failed");
        return -1;
    }
    rte_mempool_free(mp);
    return 1;
}





/******************************************************************************/
/* functions */
/******************************************************************************/

struct Mempool_t *mempool_create(int csize, uint32_t p_pool_size ,uint32_t s_pool_size, int core_num){
    int i = 0;
    struct Mempool_t *local = (struct Mempool_t *)malloc(sizeof(struct Mempool_t));

    if(local == NULL){
        return NULL;
    }
    const struct rte_memzone *mz;
    struct rte_mempool *mp;
    char mz_name[RTE_MEMZONE_NAMESIZE];
    int chunk_size;
    int zone_size;
    int numa_id;
    int chunk_num;

    int chunk_num_p;
    int chunk_num_s;
    int ret;
    int cache_size = 1;//RTE_MEMPOOL_CACHE_MAX_SIZE;
//    int cache_size = RTE_MEMPOOL_CACHE_MAX_SIZE;
/*first check Parameters*/
    if(core_num >= MAX_CORE){
        TRACE_EXCP("core_num is biger than 100 and get mempool_t failed");
        return NULL;
    }
    //TRACE_MEMORY("================in this test csize  is %d n",csize);
    chunk_size = sizeof(struct chunk_head) + csize;//sizeof(struct tcp_stream);
    //TRACE_MEMORY("we choose type_2 and  type 2 size is %d\n",chunk_size);
    if(chunk_size%64 != 0){
        chunk_size = (chunk_size/64 + 1) * 64;
    }
    TRACE_MEMORY("====chunk size is %d and  core  num is %d\n",chunk_size,core_num);
    if(chunk_size == -1){
        TRACE_EXCP("chunk size wrong \n");
        return NULL;
    } 

    if(p_pool_size  < 0 ){
        return NULL;
    } 
    if(s_pool_size < 0){    
        return NULL;
    } 
    zone_size = p_pool_size * core_num + s_pool_size + 1000;
    if(zone_size <= 0){
        return NULL;
    }
/*config local's Parameters*/    
//    local = &mempool_t[type];
//    local = &mempool_t[csize%MAX_TYPES];
    local->type = 2;
    local->size = csize;
    ret = init_chunk_functions(local,local->type);
        if(ret != 1){
            local->chunk_init = NULL;
            local->chunk_get = NULL;
            local->chunk_free = NULL;
        /*init mempool func fail and return null*/
        return NULL;
    }else{
        TRACE_MEMORY("chunk functiosn init success\n");

    } 
    local->max_core = core_num;
    local->size = chunk_size;
    for(i = 0;i< MAX_CPUS;i++){
        local->s_len[i] = 0;
        local->s_pkt[i] = NULL;
        local->s_hpkt[i] = NULL;

        local->l_len[i] = 0;
        local->l_pkt[i] = NULL;
        local->l_hpkt[i] = NULL;
    }
#if 0
/*get memzone space from hugepage*/
//only for private mempool

    for ( i = 0;i < core_num;i++){
        if(i < core_num){
            numa_id = rte_lcore_to_socket_id(i);
        }else{
            numa_id = SOCKET_ID_ANY;
        }
        //get target core nume id
        snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "MP_PRIVATE_INFO_%d_TYPE_%d", i,type);
        mz = rte_memzone_get(mz_name, zone_size,numa_id, 0);
        local->mz[i] = mz;
    } 
#endif
/* init memzone and
 * get mempool space from each memzone*/
    for( i = 0;i <  core_num + 1;i++){
    //    TRACE_MEMORY("in this loop i is %d\n ",i);
//        snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "MP_MEMZONE_INFO_%d_TYPE_%d", i,type);
//        chunk_num = p_pool_size / chunk_size + 1000;
        chunk_num_s = s_pool_size;// / chunk_size + 1000;
        chunk_num_p = p_pool_size;// / chunk_size + 1000;
#if 0
        if(chunk_num_p > 1000000){   //p_pool_size < 100MB or p_pool_size > 1GB
            chunk_num = 1000000;
        }
#endif
        //TRACE_MEMORY("chunk_num is %d \n",chunk_num);
        if(i < core_num){
            snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "PRIVATE_%d_CSIZE_%d", i+rand()%100, csize + rand()%100);
            //inorder to find right memzone,“MP_” + mempool's name == targe memzone's name
                    //TRACE_INFO("for private pool i is %d \n",i);

            mp =  rte_chunk_pool_create(mz_name, chunk_num_p,cache_size, chunk_size , core_num ,local);//this is private mempool
               if(mp == NULL){
                      TRACE_EXIT("mp is null 1\n");
               }
        }else{
            snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "PUBLIC_%d_CSIEZE_%d", -1 +rand()%100,csize + rand()%100);

            mp =  rte_chunk_pool_create(mz_name, chunk_num_s,cache_size, chunk_size , -1 ,local); //this is public mempool
            if(mp == NULL){
                TRACE_EXIT("========mp is null 2===============\n");
			}
        }
        
        if(mp != NULL ){
            if(i != core_num){
                local->mempool_p[i] = mp;
                local->mz[i] = mp->mz;
            }
            else{
                local->mempool_s = mp;
                local->mz[i] = mp->mz;
            }
            //TRACE_MEMORY("mp->ops_index is %d and i is %d \n",mp->ops_index,i);
        }
    }
        //TRACE_MEMORY("retrun local's addreess 0x%x\n!!!",local);
/*return local's address */
    return local;
}

void * mempool_alloc_chunk( struct Mempool_t* mp_t,int core_id){
    void* target = NULL;
    int type = mp_t->type;
    if(type < 0 || type >= MAX_TYPES){
        TRACE_INFO("type wrong and get mempool_t failed");
        return NULL;
    }
    if(mp_t == NULL){
           return NULL;
    }
    
    if(mp_t->chunk_get == NULL){
    //    TRACE_INFO("mp_t ->chunk_get functions is null 0x%x  and mp_t is 0x%x \n",mp_t->chunk_get,mp_t);
        return NULL;
    }
    mp_t->cur_id = core_id;
    target = (*mp_t->chunk_get)(mp_t,mp_t->mempool_p[core_id],1,core_id);
    //try to get from private pool
    if(target == NULL){
    //try to get from public mempool
        target = (*mp_t->chunk_get)(mp_t,mp_t->mempool_s,100,core_id);
    }
    return target;
}

 int mempool_free_chunk(struct Mempool_t *mp_t, void *chunk,int core_id){
    if(mp_t == NULL || chunk == NULL){
        return -1;
    }
    int ret = 0;
    
        if(mp_t == NULL){
 //           TRACE_INFO("mp_t is null in mempool_alloc_chunk\n");
           return NULL;
        }

    if(mp_t->chunk_free == NULL){
 //           TRACE_INFO("mp_t ->chunk_free functions is null and mp_t is 0x%x \n",mp_t);
        return NULL;
    }

    ret  = (*mp_t->chunk_free)(mp_t,chunk,core_id);
    return ret;
}

int mempool_destroy(struct Mempool_t* mp_t){
    return -1;
}


/*----------------------------------------------------------------------------*/
