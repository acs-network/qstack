

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
#define MAX_TYPES 6
#define RTE_MBUF_TYPE
#define RTE_MBUF_SIZE sizeof(struct rte_mbuf)
#define TYPE_SIZE_1 sizeof(struct data_type_1)
#define TYPE_SIZE_2 sizeof(struct chunk_head)
#define TYPE_SIZE_3 sizeof(struct data_type_3)
#define TYPE_SIZE_4 sizeof(struct data_type_4)

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

#include "./include/dpdk_chunk.h"

/******************************************************************************/
/* Struct data type for chunk test */
/* this is only for local test */
/******************************************************************************/
#if 1
#if 0
static const struct func_list data_tye0 = {
    .chunk_init = rte_pktmbuf_init,
    .chunk_get = chunk_alloc_t0,
    .chunk_free = chunk_free_t0,
};
static const struct func_list data_tye2 = {
    .chunk_init = data_type_init_notype,
    .chunk_get = chunk_alloc_notype,
    .chunk_free = chunk_free_notype,
};
#endif
#endif

/******************************************************************************/
/* API functions for chunk init */
/* this is only for local test */
/******************************************************************************/
int data_type_init_notype(struct rte_mempool *mp, int type, void *obj, int i){
    struct chunk_head *target = NULL;
   // printf("test 2 here and i is %d and type is %d\n",i,type);
    target = (struct chunk_head *)obj;
    if(target == NULL){
        return -1;
    }
    target->type = type;
    target->id = i;
    target->pool = mp;
    target->re = (void*)(obj + sizeof(struct chunk_head));
    return 2;
}
/******************************************************************************/
/* API functions for chunk alloc */
/* this is only for local test */
/******************************************************************************/
#if 0
//void* chunk_alloc_t0(struct rte_mempool *mp,int type){
void* chunk_alloc_t0(struct Mempool_t *mctx,struct rte_mempool *mp,int num,int core){
    struct rte_mbuf *targ = NULL;
    int type;
    type = mctx->type;
    if(mp == NULL){
        return NULL;
    }
    //src == 0 from private pool and src ==  1 from public pool
 //    assert(type == 0);
    if(type != 0){
        //printf("type is %d\n");
    }

    targ = rte_pktmbuf_alloc(mp);
  //      printf("alloc rte_mbuf \n");
    return targ;
}
#endif
#define ALLOC_NUM 1024
//void* chunk_alloc_t2 (struct rte_mempool *mp,int type){
void* chunk_alloc_notype(void *input,struct rte_mempool *mp,int type,int core_id){

    struct chunk_head *targ = NULL;
    struct chunk_head * point = NULL;
    struct rte_mempool_objhdr *hdr = NULL;
    struct rte_mempool *tmp;
    void *obj;
    void *obj_p[ALLOC_NUM];
    int ret = 0;
    int i = 0;
    struct Mempool_t *mctx = (struct Mempool_t *)input;

    for(i = 0;i <ALLOC_NUM;i++){
        obj_p[i] = NULL;
    }
    if(mp == NULL){
        return NULL;
    }
    if(type == 1){
        assert(mctx->mempool_p[core_id] == mp);
        targ = NULL;
        ret = rte_mempool_get(mp,&targ);
           if(targ != NULL){
            assert(targ->pool = mp);
            targ->size = mp->size;
            return targ->re;
        }
        if(targ == NULL){
            //from public pool
            targ = (struct chunk_head *)mctx->l_hpkt[core_id];
            if(targ == NULL){
                return NULL;
            }
            mctx->l_hpkt[core_id] = (void *)(targ->next);
            rte_wmb();
            mctx->l_len[core_id]--;
            targ->size = mp->size;
            return targ->re;
        }
    }else{
        //this is public pool
        assert(mctx->mempool_s == mp);
        ret = rte_mempool_get_bulk(mp, obj_p, ALLOC_NUM);
        if(ret  !=  0){
        //    printf("in public pool ret is %d and return NULL \n",ret);
            return NULL;
        }
        for(i = 0;i<ALLOC_NUM;i++){
#if 0
            hdr = RTE_PTR_SUB(obj_p[i], sizeof(*hdr));
            if(hdr->mp != mp && hdr->mp == NULL){
            //    printmctx->s_len[core]f("in public test  hdr->mp's name is %s and mp'name is  %s\n",hdr->mp->name,mp->name);
                assert(hdr->mp == mp);
            }
            //printf("in public test  hdr->mp's name is %s and i is %d\n",hdr->mp->name,i);
            //hdr->mp =  tmp;
#endif    
            targ = (struct chunk_head *)obj_p[i];
            if(targ == NULL){
                return NULL;
            }
            if(i == ALLOC_NUM -1){
                obj_p[i] = NULL;
                return targ->re;
             }
//            targ->lpool = mctx->mempool_s;
            if(mctx->l_len[core_id] == 0){
                targ->next = NULL;
                mctx->l_pkt[core_id] = targ;
                mctx->l_hpkt[core_id] = targ;
                rte_mb();
                mctx->l_len[core_id]++;
            }else{
                point = (struct chunk_head *)mctx->l_pkt[core_id];
                point->next = targ;
                mctx->l_pkt[core_id] = targ;
                targ->next = NULL;
                rte_mb();
                mctx->l_len[core_id]++;
            }
            obj_p[i] = NULL;
        }
        return NULL;
    }
    if(targ != NULL){
        return targ->re;
    }else{
        return NULL;
    }
}


/******************************************************************************/
/* API functions for chunk free */
/* this is only for local test */
/******************************************************************************/
#if 0
int  chunk_free_t0(struct Mempool_t *mctx,void *chunk,int core){
    struct rte_mbuf *targ = NULL;
    targ = (struct rte_mbuf *)chunk;
    //assert(targ->pool == mp);
    rte_pktmbuf_free(chunk);
    return -1;
}
#endif
#define TEST_ARR 1024
int  chunk_free_notype(void *input,void *chunk,int core){
    struct chunk_head *targ = NULL;
    struct chunk_head *point = NULL;
    struct chunk_head *array[TEST_ARR];
    int i = 0;
    void * chunk_t = NULL;
    void * obj_p = NULL;
    struct rte_mempool_objhdr *hdr = NULL;

    struct Mempool_t *mctx = (struct Mempool_t *)input;
//    struct tcp_stream* chunk_c = (struct tcp_stream *)chunk;
    chunk_t = (chunk - sizeof(struct chunk_head));
    targ = (struct chunk_head *)chunk_t;
    obj_p = chunk_t;
    assert(targ != NULL);
    assert(targ->pool != NULL);
    int time = 0;
    int ret = 0;
    for(i = 0;i<TEST_ARR;i++){
        array[i] = NULL;
    }
    hdr = RTE_PTR_SUB(obj_p, sizeof(*hdr));
    //if(hdr->mp != NULL){

    //    hdr->mp = targ->pool;
    //}
#if 1
    if(hdr->mp == mctx->mempool_s){
        //printf("chunk_free_t2 for a little test !!!!! and mctx->s_len is %d\n",mctx->s_len[core]);
        if(mctx->s_len[core] == 0){

        //    printf("when len is 0 put public chunk to list head and mctx->s_len is %d core is %d \n",mctx->s_len[core],core);
            targ->next = NULL;
            mctx->s_pkt[core] = targ;
            mctx->s_hpkt[core] = targ;
            point = (struct chunk_head *)mctx->s_pkt[core];
        //    rte_mempool_put(targ->pool,point);
            mctx->s_len[core]++;
            rte_mb();
        }else{
        //    printf("put public chunk to list and mctx->s_len is %d core is %d \n",mctx->s_len[core],core);
            assert(targ != NULL);
            targ->next = NULL;
            point = (struct chunk_head *)mctx->s_pkt[core];
            assert(point != NULL);
            point->next = (struct chunk_head *)targ;
            point = point->next;
            //point = (struct chunk_head *)mctx->s_pkt[core];
            ((struct chunk_head *)mctx->s_pkt[core])->next = targ;
            mctx->s_pkt[core] = targ;
            mctx->s_len[core]++;
            rte_mb();
            if(mctx->s_len[core] == TEST_ARR){
                point = (struct chunk_head *)mctx->s_hpkt[core];
                //printf("test 1 and mctx->s_len is %d \n",mctx->s_len[core]);
            //    assert(point->next == targ);
                for(i = 0;i < TEST_ARR;i++){
                    array[i] = point;
                //    printf("p->id is %d p->pool name is %s and targ->pool name is %s core is %d \n",point->id,point->pool,targ->pool,core);
                    assert(point->pool == targ->pool);
        //            rte_mempool_put(targ->pool,point);
                    point = point->next;
                    time++;
                }
            
            //rte_mempool_put(targ->pool, array[i]);
                rte_mempool_put_bulk(mctx->mempool_s, (void**)array, TEST_ARR);
                mctx->s_len[core] = 0;//mctx->s_len[core] - TEST_ARR;
                mctx->s_pkt[core] = NULL;
                mctx->s_hpkt[core] = NULL;
                time = 0;
            }
            rte_mb();
        }
    }else{
        rte_mempool_put(targ->pool, targ);
    }
#else
        rte_mempool_put(targ->pool, targ);
#endif
//    printf("targ->pool address is 0x%x \n",targ->pool);
//    printf("chunk_c address is 0x%x \n",chunk_c);
//    printf("targ->pool name is %s \n",targ->pool->name);
//    rte_mempool_put(targ->pool, targ);
    return 2;
}

/******************************************************************************/
/* API functions for chunk function init */
/* this is only for local test */
/******************************************************************************/
int init_chunk_functions(void *input,int type){

    struct Mempool_t *mp = (struct Mempool_t *)input;
    assert(mp->type == type);
//    printf("type is %d in init_chunk_functions \n",type);
#if 0
    if (type == 0){
        //printf("type 0 \n");
        mp->chunk_init = rte_pktmbuf_init;
        mp->chunk_get = chunk_alloc_t0;
        mp->chunk_free = chunk_free_t0;
    }else{
#endif
        mp->chunk_init = data_type_init_notype;
        mp->chunk_get = chunk_alloc_notype;
        mp->chunk_free = chunk_free_notype;
    return 1;
}

