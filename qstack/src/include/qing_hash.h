/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

/**
 * @file qing_hash.h
 * @brief hash functions from user-level drivers
 * @author zhangzhao (zhangzhao@ict.ac.cn)
 * @date 2019.7.4
 * @version 0.1
 * Function list: \n
 */
/*----------------------------------------------------------------------------*/
#ifndef _QING_HASH_H_
#define _QING_HASH_H_

/**
 * @file
 *
 * QING Hash Table
 */

#include <stdint.h>
#include <stddef.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_fbk_hash.h>
#include <rte_tailq.h>
//#include <rte_cuckoo_hash.h>
#include <rte_jhash.h>
//#include <rte_cmp_x86.h>
#include "circular_queue.h"

#define RETURN_IF_TRUE(cond, retval)

enum cmp_jump_table_case {
	KEY_CUSTOM = 0,
	KEY_16_BYTES,
	KEY_32_BYTES,
	KEY_48_BYTES,
	KEY_64_BYTES,
	KEY_80_BYTES,
	KEY_96_BYTES,
	KEY_112_BYTES,
	KEY_128_BYTES,
	KEY_OTHER_BYTES,
	NUM_KEY_CMP_CASES,
};

/* Functions to compare multiple of 16 byte keys (up to 128 bytes) */
static int
qing_hash_k16_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
	const __m128i k1 = _mm_loadu_si128((const __m128i *) key1);
	const __m128i k2 = _mm_loadu_si128((const __m128i *) key2);
	const __m128i x = _mm_xor_si128(k1, k2);

	return !_mm_test_all_zeros(x, x);
}

static int
qing_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k16_cmp_eq(key1, key2, key_len) ||
		qing_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len);
}

static int
qing_hash_k48_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k16_cmp_eq(key1, key2, key_len) ||
		qing_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len) ||
		qing_hash_k16_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
qing_hash_k64_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k32_cmp_eq(key1, key2, key_len) ||
		qing_hash_k32_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
qing_hash_k80_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k64_cmp_eq(key1, key2, key_len) ||
		qing_hash_k16_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
qing_hash_k96_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k64_cmp_eq(key1, key2, key_len) ||
		qing_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
qing_hash_k112_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k64_cmp_eq(key1, key2, key_len) ||
		qing_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len) ||
		qing_hash_k16_cmp_eq((const char *) key1 + 96,
				(const char *) key2 + 96, key_len);
}

static int
qing_hash_k128_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return qing_hash_k64_cmp_eq(key1, key2, key_len) ||
		qing_hash_k64_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

const static rte_hash_cmp_eq_t qcmp_jump_table[NUM_KEY_CMP_CASES] = {
	NULL,
	qing_hash_k16_cmp_eq,
	qing_hash_k32_cmp_eq,
	qing_hash_k48_cmp_eq,
	qing_hash_k64_cmp_eq,
	qing_hash_k80_cmp_eq,
	qing_hash_k96_cmp_eq,
	qing_hash_k112_cmp_eq,
	qing_hash_k128_cmp_eq,
	memcmp
};
/*
 * Table storing all different key compare functions
 * (multi-process supported)
 */
	
/** Number of items per bucket. */
#define RTE_HASH_BUCKET_ENTRIES		8

#define NULL_SIGNATURE			0

#define EMPTY_SLOT			0

#define KEY_ALIGNMENT			16

#define LCORE_CACHE_SIZE		64

#define RTE_HASH_MAX_PUSHES             100

#define RTE_HASH_BFS_QUEUE_MAX_LEN       1000

#define RTE_XABORT_CUCKOO_PATH_INVALIDED 0x4

#define RTE_HASH_TSX_MAX_RETRY  10


struct lcore_cache {
		unsigned len; /**< Cache len */
			void *objs[LCORE_CACHE_SIZE]; /**< Cache objects */
} __rte_cache_aligned;
/* Structure that stores key-value pair */
struct rte_hash_key {
		union {
					uintptr_t idata;
							void *pdata;
								};
			/* Variable key size */
			char key[0];
} __attribute__((aligned(KEY_ALIGNMENT)));
/*
 * All different options to select a key compare function,
 * based on the key size and custom function.
 */

enum add_key_case {
	ADD_KEY_SINGLEWRITER = 0,
	ADD_KEY_MULTIWRITER,
	ADD_KEY_MULTIWRITER_TM,
};

enum rte_hash_sig_compare_function {
	RTE_HASH_COMPARE_SCALAR = 0,
	RTE_HASH_COMPARE_SSE,
	RTE_HASH_COMPARE_AVX2,
	RTE_HASH_COMPARE_NUM
};
/** dummy structure type used by the rte_tailq APIs */
/** dummy */


/** Bucket structure */
struct rte_hash_bucket {
	hash_sig_t sig_current[RTE_HASH_BUCKET_ENTRIES];

	uint32_t key_idx[RTE_HASH_BUCKET_ENTRIES];

	hash_sig_t sig_alt[RTE_HASH_BUCKET_ENTRIES];

	uint8_t flag[RTE_HASH_BUCKET_ENTRIES];
} __rte_cache_aligned;

/**  A hash table structure. */
struct qing_hash {
	char name[RTE_HASH_NAMESIZE];   /**< Name of the hash. */
	uint32_t entries;               /**< Total table entries. */
	uint32_t num_buckets;           /**< Number of buckets in table. */

    struct circular_queue *free_slots;
//	struct rte_ring *free_slots;
	/**< Ring that stores all indexes of the free slots in the key table */
	uint8_t hw_trans_mem_support;
	/**< Hardware transactional memory support */
	struct lcore_cache *local_free_slots;
	/**< Local cache per lcore, storing some indexes of the free slots */
	enum add_key_case add_key; /**< Multi-writer hash add behavior */

	rte_spinlock_t *multiwriter_lock; /**< Multi-writer spinlock for w/o TM */

	/* Fields used in lookup */

	uint32_t key_len __rte_cache_aligned;
	/**< Length of hash key. */
	rte_hash_function hash_func;    /**< Function used to calculate hash. */
	uint32_t hash_func_init_val;    /**< Init value used by hash_func. */
	rte_hash_cmp_eq_t rte_hash_custom_cmp_eq;
	/**< Custom function used to compare keys. */
	enum cmp_jump_table_case cmp_jump_table_idx;
	/**< Indicates which compare function to use. */
	enum rte_hash_sig_compare_function sig_cmp_fn;
	/**< Indicates which signature compare function to use. */
	uint32_t bucket_bitmask;
	/**< Bitmask for getting bucket index from hash signature. */
	uint32_t key_entry_size;         /**< Size of each key entry. */

	void *key_store;                /**< Table storing all keys and data */
	struct rte_hash_bucket *buckets;
	/**< Table with buckets storing all the	hash values and key indexes
	 * to the key table.
	 */
} __rte_cache_aligned;
/** @internal A hash table structure. */

struct queue_node {
		struct rte_hash_bucket *bkt; /* Current bucket on the bfs search */

			struct queue_node *prev;     /* Parent(bucket) in search path */
				int prev_slot;               /* Parent(slot) in search path */
};


/**
 * allocate all memory used by hash table.
 * @param h
 *   create a  new Hash table 
 */

struct qing_hash *
qing_hash_create(const struct rte_hash_parameters *params);

/**
 * De-allocate all memory used by hash table.
 * @param h
 *   Hash table to free
 */
void
qing_hash_free(struct qing_hash *h);


/**
 * Add a key-value pair to an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread.
 *
 * @param h
 *   Hash table to add the key to.
 * @param key
 *   Key to add to the hash table.
 * @param data
 *   Data to add to the hash table.
 * @return
 *   - 0 if added successfully
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOSPC if there is no space in the hash for this key.
 */
int
qing_hash_add_key_data(const struct qing_hash *h, const void *key, void *data);


/**
 * Remove a key from an existing hash table.
 * This operation is not multi-thread safe
 * and should only be called from one thread.
 *
 * @param h
 *   Hash table to remove the key from.
 * @param key
 *   Key to remove from the hash table.
 * @return
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 *   - A positive value that can be used by the caller as an offset into an
 *     array of user data. This value is unique for this key, and is the same
 *     value that was returned when the key was added.
 */
int32_t
qing_hash_del_key(const struct qing_hash *h, const void *key);


/**
 * Find a key-value pair in the hash table.
 * This operation is multi-thread safe.
 * @param h
 *   Hash table to look in.
 * @param key
 *   Key to find.
 * @param data
 *   Output with pointer to data returned from the hash table.
 * @return
 *   0 if successful lookup
 *   - EINVAL if the parameters are invalid.
 *   - ENOENT if the key is not found.
 */
int
qing_hash_lookup_data(const struct qing_hash *h, const void *key, void **data);

#endif /* _QING_HASH_H_ */

