/** 
 * @file hashtable.c
 * @brief hastable for stream find with five-tuples
 * @author Song Hui (songhui@ict.ac.cn) 
 * @date 2018.8.22
 * @version 1.0
 * @detail Function list: \n
 *	1.ht_create(): config hash parametres and allocate hashtable memory \n
 *	2.ht_insert(): insert a key-value pair into hashtable \n
 *	3.ht_search(): lookup value with hash key in hashtable \n
 *	4.ht_delete(): delete a key-value pair in hashtable \n
 *  5.ht_destroy():destroy the hashtable
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *	1. Date: 2018.8.19 
 *	   Author: Song Hui
 *	   Modification: create
 */ 
/******************************************************************************/
#include "hashtable.h"
/*** ***************************************************************************/

#define SUCCESS 1
#define ERROR   0

#define STREAM_HT_BUCKET 16;

int ht_idx;

/**
 * config params for initial hash table.
  */ 
struct rte_hash_parameters ipv4_hash_params = {
                .name = NULL,
				//.bucket_entries = STREAM_HT_BUCKET,
                .key_len = sizeof(struct ipv4_5tuple),
                .hash_func = rte_hash_crc,//DEFAULT_HASH_FUNC,
                .hash_func_init_val = 0,
}; 

/* * 
 * config hash parametres and allocate hashtable memory
 * @param[in]	 num_bins  	max entries of hashtable
 * @return		 hashtable
 * @ref 		 hashtable.h
 * @see
 * @note
 */ 
hashtable_t
ht_create( int num_bins)
{
	char s[64];
	hashtable_t stream_ht;
//	struct qing_hash * p = NULL;
    /* create hashtables */
    snprintf(s, sizeof(s), "ipv4_hashtable_%d",ht_idx++);
    ipv4_hash_params.name = s;
    ipv4_hash_params.entries = num_bins;
    stream_ht = rte_hash_create(&ipv4_hash_params);
//    p = qing_hash_create(&ipv4_hash_params);
//	stream_ht = p;
//	TRACE_INFO("stream ht address is %p in ht_create and p is %p \n",stream_ht,p);
    if (stream_ht == NULL)
    	rte_exit(EXIT_FAILURE, "Unable to create ht %s\n", s);
    else
    	printf("Success create ht %s and return %p \n",s,stream_ht);
	return stream_ht;
}

 /** 
 * insert a key-value pair into hashtable
 * @param[in]	 ht  	hashtable
 * @param[in]	 key  	hash key
 * @param[in]	 value  hash value
 * @return		 SUCCESS if success;ERROR if fails
 * @ref 		 hashtable.h
 * @see
 * @note
 */
int 
ht_insert(hashtable_t ht, void *key, void *value)
{ 
	int err;
	err = rte_hash_add_key_data(ht,key,value);
//	err = qing_hash_add_key_data(ht,key,value);
 	if(err >=0 ){
		TRACE_STREAM("HT_INSERT:hash index:%d.\n",err);
		return err;
	}else if(err == -EINVAL){
		TRACE_EXCP("HT_INSERT:Params don't exist!\n");
		return err;
	}else if(err == -ENOSPC){
		TRACE_EXCP("HT_INSERT:Not enough hashtable memory!\n");
		return err;
	}
}

/** 
 * lookup value with hash key in hashtable
 * @param[in]	 ht  	hashtable
 * @param[in]	 key  	hash key
 * @return		 hash value
 * @ref 		 hashtable.h
 * @see
 * @note
 */
void *
ht_search(hashtable_t ht, void *key)
{
	int err;
	void *value;
	err = rte_hash_lookup_data(ht,key,&value);
//	err = qing_hash_lookup_data(ht,key,&value);
	if(err < 0){
		if (err == -EINVAL){
			TRACE_EXCP("HT_SEARCH: Params don't exist!\n");	
		}else if(err == -ENOENT){
			//printf("HT_SEARCH:ht name:%s,ht entry:%d,ht key:%p.\n",ht->name,ht->entries,ht->key);
			TRACE_STREAM("HT_SEARCH:Can not find the key,search fails!\n");
		}
		return NULL;
	}else
		return value;
}

/** 
 * delete a key-value pair in hashtable
 * @param[in]	 ht  	hashtable
 * @param[in]	 key  	hash key
 * @return		 SUCCESS if success;ERROR if fails
 * @ref 		 hashtable.h
 * @see
 * @note
 */
int 
ht_delete(hashtable_t ht, void *key)
{
	int err;
//	err = qing_hash_del_key(ht, key);
	err = rte_hash_del_key(ht, key);
	if(!err)
		return SUCCESS;
	else if(err == -EINVAL){
		TRACE_EXCP("HT_DELETE:Params don't exist!\n");
		return ERROR;
	}else if(err == -ENOENT){
		TRACE_EXCP("HT_DELETE:Can not find the key,delete fails!\n");
		return ERROR;
	}else
		return 1;
}

/** 
 * destroy the hashtable
 * @param[in]	 ht  	hashtable
 * @ref 		 hashtable.h
 * @see
 * @note
 */
void 
ht_destroy(hashtable_t ht)
{
	rte_hash_free(ht);
//	qing_hash_free(ht);
}
