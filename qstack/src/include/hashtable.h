/**
 * @file hashtable.h
 * @brief hastable for stream find with five-tuples
 * @author Song Hui (songhui@ict.ac.cn) 
 * @date 2018.8.22 
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *	1. Date: 2018.8.22
 *	   Author: Song Hui
 *	   Modification: create
 */
/******************************************************************************/

#ifndef __HASHTABLE_H_
#define __HASHTABLE_H_
/******************************************************************************/
#include <rte_fbk_hash.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include "universal.h"
//#include "qing_hash.h"
/******************************************************************************/

#define DEFAULT_HASH_FUNC  rte_jhash

/**
 * Structure of 5-tuples in tcp packets for hash key.
 */
struct ipv4_5tuple 
{
	uint8_t  proto;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
};
typedef struct ipv4_5tuple *qtuple_t;


/**
 * Structure of hashtable.
 */

//struct qing_hash {
typedef struct qing_hash * hashtable_t;
//typedef struct rte_hash *hashtable_t;

/** 
 * config hash parametres and allocate hashtable memory
 * @param[in]	 num_bins  	max entries of hashtable
 * @return		 hashtable
 * @ref 		 hashtable.h
 * @see
 * @note
 */
hashtable_t
ht_create( int num_bins);

/** 
 * insert a key-value pair into hashtable
 * @param[in]	 ht  	hashtable
 * @param[in]	 key  	hash key
 * @param[in]	 value  hash value
 * @return		 hashtable
 * @ref 		 hashtable.h
 * @see
 * @note
 */
int 
ht_insert(hashtable_t ht, void *key, void *value);

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
ht_search(hashtable_t ht, void *key);

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
ht_delete(hashtable_t ht, void *key);

/** 
 * destroy the hashtable
 * @param[in]	 ht  	hashtable
 * @ref 		 hashtable.h
 * @see
 * @note
 */
void 
ht_destroy(hashtable_t ht);

#endif


