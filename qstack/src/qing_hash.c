#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/queue.h>


#include <rte_common.h>
#include <rte_memory.h>         /* for definition of RTE_CACHE_LINE_SIZE */
#include <rte_log.h>
#include <rte_memcpy.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_rwlock.h>
#include <rte_spinlock.h>
#include <rte_ring.h>
#include <rte_compat.h>
#include <rte_pause.h>
#include <rte_hash.h>
#include <rte_fbk_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>


#include "universal.h"
#include "qing_hash.h"
#include "qstack.h"
#include "circular_queue.h"

TAILQ_HEAD(qing_hash_lish, rte_tailq_entry) qing_queue_head;

//static struct rte_tailq_elem qing_hash_tailq = {
//		.name = "QING_HASH",
//};
//EAL_REGISTER_TAILQ(qing_hash_tailq)
/* add inline functions */
static inline int
rte_hash_cmp_eq(const void *key1, const void *key2, const struct qing_hash *h)
{
//	if (h->cmp_jump_table_idx == KEY_CUSTOM)
//		return h->rte_hash_custom_cmp_eq(key1, key2, h->key_len);
//	else
		return qcmp_jump_table[h->cmp_jump_table_idx](key1, key2, h->key_len);
}

/* Calc the secondary hash value from the primary hash value of a given key */
static inline hash_sig_t
rte_hash_secondary_hash(const hash_sig_t primary_hash)
{
	static const unsigned all_bits_shift = 12;
	static const unsigned alt_bits_xor = 0x5bd1e995;

	uint32_t tag = primary_hash >> all_bits_shift;

	return primary_hash ^ ((tag + 1) * alt_bits_xor);
}

/* Only tries to insert at one bucket (@prim_bkt) without trying to push
 * buckets around
 */
static inline unsigned
rte_hash_cuckoo_insert_mw_tm(struct rte_hash_bucket *prim_bkt,
		hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx)
{
	unsigned i, status;
	unsigned try = 0;

	while (try < RTE_HASH_TSX_MAX_RETRY) {
		status = rte_xbegin();
		if (likely(status == RTE_XBEGIN_STARTED)) {
			/* Insert new entry if there is room in the primary
			* bucket.
			*/
			for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
				/* Check if slot is available */
				if (likely(prim_bkt->key_idx[i] == EMPTY_SLOT)) {
					prim_bkt->sig_current[i] = sig;
					prim_bkt->sig_alt[i] = alt_hash;
					prim_bkt->key_idx[i] = new_idx;
					break;
				}
			}
			rte_xend();

			if (i != RTE_HASH_BUCKET_ENTRIES)
				return 0;

			break; /* break off try loop if transaction commits */
		} else {
			/* If we abort we give up this cuckoo path. */
			try++;
			rte_pause();
		}
	}

	return -1;
}

/* Shift buckets along provided cuckoo_path (@leaf and @leaf_slot) and fill
 * the path head with new entry (sig, alt_hash, new_idx)
 */
static inline int
rte_hash_cuckoo_move_insert_mw_tm(const struct qing_hash *h,
			struct queue_node *leaf, uint32_t leaf_slot,
			hash_sig_t sig, hash_sig_t alt_hash, uint32_t new_idx)
{
	unsigned try = 0;
	unsigned status;
	uint32_t prev_alt_bkt_idx;

	struct queue_node *prev_node, *curr_node = leaf;
	struct rte_hash_bucket *prev_bkt, *curr_bkt = leaf->bkt;
	uint32_t prev_slot, curr_slot = leaf_slot;

	while (try < RTE_HASH_TSX_MAX_RETRY) {
		status = rte_xbegin();
		if (likely(status == RTE_XBEGIN_STARTED)) {
			while (likely(curr_node->prev != NULL)) {
				prev_node = curr_node->prev;
				prev_bkt = prev_node->bkt;
				prev_slot = curr_node->prev_slot;

				prev_alt_bkt_idx
					= prev_bkt->sig_alt[prev_slot]
					    & h->bucket_bitmask;

				if (unlikely(&h->buckets[prev_alt_bkt_idx]
					     != curr_bkt)) {
					rte_xabort(RTE_XABORT_CUCKOO_PATH_INVALIDED);
				}

				/* Need to swap current/alt sig to allow later
				 * Cuckoo insert to move elements back to its
				 * primary bucket if available
				 */
				curr_bkt->sig_alt[curr_slot] =
				    prev_bkt->sig_current[prev_slot];
				curr_bkt->sig_current[curr_slot] =
				    prev_bkt->sig_alt[prev_slot];
				curr_bkt->key_idx[curr_slot]
				    = prev_bkt->key_idx[prev_slot];

				curr_slot = prev_slot;
				curr_node = prev_node;
				curr_bkt = curr_node->bkt;
			}

			curr_bkt->sig_current[curr_slot] = sig;
			curr_bkt->sig_alt[curr_slot] = alt_hash;
			curr_bkt->key_idx[curr_slot] = new_idx;

			rte_xend();

			return 0;
		}

		/* If we abort we give up this cuckoo path, since most likely it's
		 * no longer valid as TSX detected data conflict
		 */
		try++;
		rte_pause();
	}

	return -1;
}

/*
 * Make space for new key, using bfs Cuckoo Search and Multi-Writer safe
 * Cuckoo
 */
static inline int
rte_hash_cuckoo_make_space_mw_tm(const struct qing_hash *h,
			struct rte_hash_bucket *bkt,
			hash_sig_t sig, hash_sig_t alt_hash,
			uint32_t new_idx)
{
	unsigned i;
	struct queue_node queue[RTE_HASH_BFS_QUEUE_MAX_LEN];
	struct queue_node *tail, *head;
	struct rte_hash_bucket *curr_bkt, *alt_bkt;

	tail = queue;
	head = queue + 1;
	tail->bkt = bkt;
	tail->prev = NULL;
	tail->prev_slot = -1;

	/* Cuckoo bfs Search */
	while (likely(tail != head && head <
					queue + RTE_HASH_BFS_QUEUE_MAX_LEN -
					RTE_HASH_BUCKET_ENTRIES)) {
		curr_bkt = tail->bkt;
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			if (curr_bkt->key_idx[i] == EMPTY_SLOT) {
				if (likely(rte_hash_cuckoo_move_insert_mw_tm(h,
						tail, i, sig,
						alt_hash, new_idx) == 0))
					return 0;
			}

			/* Enqueue new node and keep prev node info */
			alt_bkt = &(h->buckets[curr_bkt->sig_alt[i]
						    & h->bucket_bitmask]);
			head->bkt = alt_bkt;
			head->prev = tail;
			head->prev_slot = i;
			head++;
		}
		tail++;
	}

	return -ENOSPC;
} 

/* Search for an entry that can be pushed to its alternative location */
static inline int
make_space_bucket(const struct qing_hash *h, struct rte_hash_bucket *bkt,
		unsigned int *nr_pushes)
{
	unsigned i, j;
	int ret;
	uint32_t next_bucket_idx;
	struct rte_hash_bucket *next_bkt[RTE_HASH_BUCKET_ENTRIES];

	/*
	 * Push existing item (search for bucket with space in
	 * alternative locations) to its alternative location
	 */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		/* Search for space in alternative locations */
		next_bucket_idx = bkt->sig_alt[i] & h->bucket_bitmask;
		next_bkt[i] = &h->buckets[next_bucket_idx];
		for (j = 0; j < RTE_HASH_BUCKET_ENTRIES; j++) {
			if (next_bkt[i]->key_idx[j] == EMPTY_SLOT)
				break;
		}

		if (j != RTE_HASH_BUCKET_ENTRIES)
			break;
	}

	/* Alternative location has spare room (end of recursive function) */
	if (i != RTE_HASH_BUCKET_ENTRIES) {
		next_bkt[i]->sig_alt[j] = bkt->sig_current[i];
		next_bkt[i]->sig_current[j] = bkt->sig_alt[i];
		next_bkt[i]->key_idx[j] = bkt->key_idx[i];
		return i;
	}

	/* Pick entry that has not been pushed yet */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++)
		if (bkt->flag[i] == 0)
			break;

	/* All entries have been pushed, so entry cannot be added */
	if (i == RTE_HASH_BUCKET_ENTRIES || ++(*nr_pushes) > RTE_HASH_MAX_PUSHES)
		return -ENOSPC;

	/* Set flag to indicate that this entry is going to be pushed */
	bkt->flag[i] = 1;

	/* Need room in alternative bucket to insert the pushed entry */
	ret = make_space_bucket(h, next_bkt[i], nr_pushes);
	/*
	 * After recursive function.
	 * Clear flags and insert the pushed entry
	 * in its alternative location if successful,
	 * or return error
	 */
	bkt->flag[i] = 0;
	if (ret >= 0) {
		next_bkt[i]->sig_alt[ret] = bkt->sig_current[i];
		next_bkt[i]->sig_current[ret] = bkt->sig_alt[i];
		next_bkt[i]->key_idx[ret] = bkt->key_idx[i];
		return i;
	} else
		return ret;

}

/* inline functions over */

struct qing_hash *
qing_hash_create(const struct rte_hash_parameters *params)
{
    struct qing_hash *h = NULL;
	struct rte_tailq_entry *te = NULL;
	struct qing_hash_lish *hash_list;
    struct circular_queue *r = NULL;
	char hash_name[RTE_HASH_NAMESIZE];
	void *k = NULL;
	void *buckets = NULL;
	char ring_name[RTE_RING_NAMESIZE];
	unsigned num_key_slots;
	unsigned hw_trans_mem_support = 0;
	unsigned i;
	TAILQ_INIT(&qing_queue_head);
	rte_hash_function default_hash_func = (rte_hash_function)rte_jhash;

	hash_list = &qing_queue_head;//RTE_TAILQ_CAST(qing_hash_tailq.head, qing_hash_lish);
//get head
	if (params == NULL) {
		RTE_LOG(ERR, HASH, "rte_hash_create has no parameters\n");
		return NULL;
	}

	/* Check for valid parameters */
	if ((params->entries > RTE_HASH_ENTRIES_MAX) ||
			(params->entries < RTE_HASH_BUCKET_ENTRIES) ||
			!rte_is_power_of_2(RTE_HASH_BUCKET_ENTRIES) ||
			(params->key_len == 0)) {
		rte_errno = EINVAL;
		RTE_LOG(ERR, HASH, "rte_hash_create has invalid parameters\n");
		return NULL;
	}

	/* Check extra flags field to check extra options. */
	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT)
		hw_trans_mem_support = 1;

	/* Store all keys and leave the first entry as a dummy entry for lookup_bulk */
	if (hw_trans_mem_support)
		/*
		 * Increase number of slots by total number of indices
		 * that can be stored in the lcore caches
		 * except for the first cache
		 */
		num_key_slots = params->entries + (RTE_MAX_LCORE - 1) *
					LCORE_CACHE_SIZE + 1;
	else
		num_key_slots = params->entries + 1;

	snprintf(ring_name, sizeof(ring_name), "HT_%s", params->name);
	/* Create ring (Dummy slot index is not enqueued) */
	//r = rte_ring_create(ring_name, rte_align32pow2(num_key_slots - 1),
	//		params->socket_id, 0);
	
	r = malloc(sizeof(struct circular_queue));
	memset(r,0,sizeof(struct circular_queue));
    cirq_init(r, rte_align32pow2(num_key_slots - 1));
	if (r == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err;
	}

	snprintf(hash_name, sizeof(hash_name), "HT_%s", params->name);

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* guarantee there's no existing: this is normally already checked
	 * by ring creation above */
	TAILQ_FOREACH(te, hash_list, next) {
		h = (struct qing_hash *) te->data;
		if (strncmp(params->name, h->name, RTE_HASH_NAMESIZE) == 0)
			break;
	}
	h = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		te = NULL;
		goto err_unlock;
	}

	te = rte_zmalloc("HASH_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, HASH, "tailq entry allocation failed\n");
		goto err_unlock;
	}

	//h = (struct qing_hash *)rte_zmalloc_socket(hash_name, sizeof(struct qing_hash),
	//				RTE_CACHE_LINE_SIZE, params->socket_id);
    h = malloc(sizeof(struct qing_hash));
	if (h == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}else
	{
		TRACE_INFO("h is %p in qing_hash_create \n",h);
	}

	const uint32_t num_buckets = rte_align32pow2(params->entries)
					/ RTE_HASH_BUCKET_ENTRIES;

	buckets = rte_zmalloc_socket(NULL,
				num_buckets * sizeof(struct rte_hash_bucket),
				RTE_CACHE_LINE_SIZE, params->socket_id);

	if (buckets == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}

	const uint32_t key_entry_size = sizeof(struct rte_hash_key) + params->key_len;
	const uint64_t key_tbl_size = (uint64_t) key_entry_size * num_key_slots;

	k = rte_zmalloc_socket(NULL, key_tbl_size,
			RTE_CACHE_LINE_SIZE, params->socket_id);

	if (k == NULL) {
		RTE_LOG(ERR, HASH, "memory allocation failed\n");
		goto err_unlock;
	}

/*
 * If x86 architecture is used, select appropriate compare function,
 * which may use x86 intrinsics, otherwise use memcmp
 */
#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	/* Select function to compare keys */
	switch (params->key_len) {
	case 16:
		h->cmp_jump_table_idx = KEY_16_BYTES;
		break;
	case 32:
		h->cmp_jump_table_idx = KEY_32_BYTES;
		break;
	case 48:
		h->cmp_jump_table_idx = KEY_48_BYTES;
		break;
	case 64:
		h->cmp_jump_table_idx = KEY_64_BYTES;
		break;
	case 80:
		h->cmp_jump_table_idx = KEY_80_BYTES;
		break;
	case 96:
		h->cmp_jump_table_idx = KEY_96_BYTES;
		break;
	case 112:
		h->cmp_jump_table_idx = KEY_112_BYTES;
		break;
	case 128:
		h->cmp_jump_table_idx = KEY_128_BYTES;
		break;
	default:
		/* If key is not multiple of 16, use generic memcmp */
		h->cmp_jump_table_idx = KEY_OTHER_BYTES;
	}
#else
	h->cmp_jump_table_idx = KEY_OTHER_BYTES;
#endif

	if (hw_trans_mem_support) {
		h->local_free_slots = rte_zmalloc_socket(NULL,
				sizeof(struct lcore_cache) * RTE_MAX_LCORE,
				RTE_CACHE_LINE_SIZE, params->socket_id);
	}

	/* Default hash function */
//#if defined(RTE_ARCH_X86)
	default_hash_func = (rte_hash_function)rte_hash_crc;
//#elif defined(RTE_ARCH_ARM64)
//	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_CRC32))
//		default_hash_func = (rte_hash_function)rte_hash_crc;
//#endif
	/* Setup hash context */
	snprintf(h->name, sizeof(h->name), "%s", params->name);
	h->entries = params->entries;
	h->key_len = params->key_len;
	h->key_entry_size = key_entry_size;
	h->hash_func_init_val = params->hash_func_init_val;

	h->num_buckets = num_buckets;
	h->bucket_bitmask = h->num_buckets - 1;
	h->buckets = buckets;
	h->hash_func = (params->hash_func == NULL) ?
		default_hash_func : params->hash_func;
	h->key_store = k;
	h->free_slots = r;
	h->hw_trans_mem_support = hw_trans_mem_support;

#if defined(RTE_ARCH_X86)
	if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		h->sig_cmp_fn = RTE_HASH_COMPARE_AVX2;
	else if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_SSE2))
		h->sig_cmp_fn = RTE_HASH_COMPARE_SSE;
	else
#endif
		h->sig_cmp_fn = RTE_HASH_COMPARE_SCALAR;

	/* Turn on multi-writer only with explicit flat from user and TM
	 * support.
	 */
	if (params->extra_flag & RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD) {
		if (h->hw_trans_mem_support) {
			h->add_key = ADD_KEY_MULTIWRITER_TM;
		} else {
			h->add_key = ADD_KEY_MULTIWRITER;
			h->multiwriter_lock = rte_malloc(NULL,
							sizeof(rte_spinlock_t),
							LCORE_CACHE_SIZE);
			rte_spinlock_init(h->multiwriter_lock);
		}
	} else
		h->add_key = ADD_KEY_SINGLEWRITER;

	/* Populate free slots ring. Entry zero is reserved for key misses. */
	for (i = 1; i < params->entries + 1; i++)
	{
	//	rte_ring_sp_enqueue(r, (void *)((uintptr_t) i));
        cirq_add(r, (void *)((uintptr_t) i));
    }
	te->data = (void *) h;
	TAILQ_INSERT_TAIL(hash_list, te, next);
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
    
	TRACE_INFO(" qing_hash_create return is %p \n",h);
	return h;
err_unlock:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
err:
    free(r->queue);
	free(r);
	rte_free(te);
	rte_free(h);
	rte_free(buckets);
	rte_free(k);
	return NULL;

}


void
qing_hash_free(struct qing_hash *h)
{ 
    struct rte_tailq_entry *te;
	struct qing_hash_lish *hash_list;

	if (h == NULL)
		return;

	hash_list = &qing_queue_head;//RTE_TAILQ_CAST(qing_hash_tailq.head, qing_hash_lish);
//get head
	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/* find out tailq entry */
 	TAILQ_FOREACH(te, hash_list, next) {
		if (te->data == (void *) h)
			break;
	}

 	if (te == NULL) {
		rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
		return;
	}

	TAILQ_REMOVE(hash_list, te, next);

	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (h->hw_trans_mem_support)
		rte_free(h->local_free_slots);

	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_free(h->multiwriter_lock);
	free(h->free_slots->queue);
	free(h->free_slots);
    h->free_slots = NULL;
	rte_free(h->key_store);
	rte_free(h->buckets);
	free(h);
	rte_free(te);
}
static inline void
enqueue_slot_back(const struct qing_hash *h,
		struct lcore_cache *cached_free_slots,
		void *slot_id)
{
	if (h->hw_trans_mem_support) {
		cached_free_slots->objs[cached_free_slots->len] = slot_id;
		cached_free_slots->len++;
	} else
		cirq_add(h->free_slots, slot_id);
}

static inline int32_t
__qing_hash_add_key_with_hash(const struct qing_hash *h, const void *key,
						hash_sig_t sig, void *data)
{
	hash_sig_t alt_hash;
	uint32_t prim_bucket_idx, sec_bucket_idx;
	unsigned i;
    int keyid;
	struct rte_hash_bucket *prim_bkt, *sec_bkt;
	struct rte_hash_key *new_k, *k, *keys = h->key_store;
	void *slot_id = NULL;
	uint32_t new_idx;
	int ret;
	unsigned n_slots;
	unsigned lcore_id;
	struct lcore_cache *cached_free_slots = NULL;
	unsigned int nr_pushes = 0;

	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_spinlock_lock(h->multiwriter_lock);

	prim_bucket_idx = sig & h->bucket_bitmask;
	prim_bkt = &h->buckets[prim_bucket_idx];
	rte_prefetch0(prim_bkt);

	alt_hash = rte_hash_secondary_hash(sig);
	sec_bucket_idx = alt_hash & h->bucket_bitmask;
	sec_bkt = &h->buckets[sec_bucket_idx];
	rte_prefetch0(sec_bkt);

	/* Get a new slot for storing the new key */
	if (h->hw_trans_mem_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Try to get a free slot from the local cache */
		if (cached_free_slots->len == 0) {
			/* Need to get another burst of free slots from global ring */
            n_slots = 0;
            for(keyid =0;keyid<LCORE_CACHE_SIZE;keyid++)
            {
                cached_free_slots->objs[n_slots] = cirq_get(h->free_slots);
                if(cached_free_slots->objs[n_slots] != NULL)
				{
					n_slots++;
				}
            }
			//n_slots = rte_ring_mc_dequeue_burst(h->free_slots,
			//		cached_free_slots->objs,
			//		LCORE_CACHE_SIZE, NULL);
			if (n_slots == 0) {
				ret = -ENOSPC;
				goto failure;
			}

			cached_free_slots->len += n_slots;
		}

		/* Get a free slot from the local cache */
		cached_free_slots->len--;
		slot_id = cached_free_slots->objs[cached_free_slots->len];
	} else {
	//	if (rte_ring_sc_dequeue(h->free_slots, &slot_id) != 0) {
	//		ret = -ENOSPC;
	//		goto failure;
	//	}
	    slot_id = cirq_get(h->free_slots);
		if(slot_id == NULL)
		{
			ret = -ENOSPC;
			goto failure;
		}
	}

	new_k = RTE_PTR_ADD(keys, (uintptr_t)slot_id * h->key_entry_size);
	rte_prefetch0(new_k);
	new_idx = (uint32_t)((uintptr_t) slot_id);

	/* Check if key is already inserted in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (prim_bkt->sig_current[i] == sig &&
				prim_bkt->sig_alt[i] == alt_hash) {
			k = (struct rte_hash_key *) ((char *)keys +
					prim_bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				/* Enqueue index of free slot back in the ring. */
				enqueue_slot_back(h, cached_free_slots, slot_id);
				/* Update data */
				k->pdata = data;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				ret = prim_bkt->key_idx[i] - 1;
				goto failure;
			}
		}
	}

	/* Check if key is already inserted in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (sec_bkt->sig_alt[i] == sig &&
				sec_bkt->sig_current[i] == alt_hash) {
			k = (struct rte_hash_key *) ((char *)keys +
					sec_bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				/* Enqueue index of free slot back in the ring. */
				enqueue_slot_back(h, cached_free_slots, slot_id);
				/* Update data */
				k->pdata = data;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				ret = sec_bkt->key_idx[i] - 1;
				goto failure;
			}
		}
	}

	/* Copy key */
	rte_memcpy(new_k->key, key, h->key_len);
	new_k->pdata = data;

#if defined(RTE_ARCH_X86) /* currently only x86 support HTM */
	if (h->add_key == ADD_KEY_MULTIWRITER_TM) {
		ret = rte_hash_cuckoo_insert_mw_tm(prim_bkt,
				sig, alt_hash, new_idx);
		if (ret >= 0)
			return new_idx - 1;

		/* Primary bucket full, need to make space for new entry */
		ret = rte_hash_cuckoo_make_space_mw_tm(h, prim_bkt, sig,
							alt_hash, new_idx);

		if (ret >= 0)
			return new_idx - 1;

		/* Also search secondary bucket to get better occupancy */
		ret = rte_hash_cuckoo_make_space_mw_tm(h, sec_bkt, sig,
							alt_hash, new_idx);

		if (ret >= 0)
			return new_idx - 1;
	} else {
#endif
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			/* Check if slot is available */
			if (likely(prim_bkt->key_idx[i] == EMPTY_SLOT)) {
				prim_bkt->sig_current[i] = sig;
				prim_bkt->sig_alt[i] = alt_hash;
				prim_bkt->key_idx[i] = new_idx;
				break;
			}
		}

		if (i != RTE_HASH_BUCKET_ENTRIES) {
			if (h->add_key == ADD_KEY_MULTIWRITER)
				rte_spinlock_unlock(h->multiwriter_lock);
			return new_idx - 1;
		}

		/* Primary bucket full, need to make space for new entry
		 * After recursive function.
		 * Insert the new entry in the position of the pushed entry
		 * if successful or return error and
		 * store the new slot back in the ring
		 */
		ret = make_space_bucket(h, prim_bkt, &nr_pushes);
		if (ret >= 0) {
			prim_bkt->sig_current[ret] = sig;
			prim_bkt->sig_alt[ret] = alt_hash;
			prim_bkt->key_idx[ret] = new_idx;
			if (h->add_key == ADD_KEY_MULTIWRITER)
				rte_spinlock_unlock(h->multiwriter_lock);
			return new_idx - 1;
		}
#if defined(RTE_ARCH_X86)
	}
#endif
	/* Error in addition, store new slot back in the ring and return error */
	enqueue_slot_back(h, cached_free_slots, (void *)((uintptr_t) new_idx));

failure:
	if (h->add_key == ADD_KEY_MULTIWRITER)
		rte_spinlock_unlock(h->multiwriter_lock);
	return ret;
}

int
qing_hash_add_key_data(const struct qing_hash *h, const void *key, void *data)
{
    int ret;

	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);
	hash_sig_t r;
    r = rte_hash_crc(key,h->key_len,h->hash_func_init_val);
	//ret = __qing_hash_add_key_with_hash(h, key, rte_hash_hash(h, key), data);
	ret = __qing_hash_add_key_with_hash(h, key, r, data);
	if (ret >= 0)
		return 0;
	else
		return ret;
} 

static inline void
remove_entry(const struct qing_hash *h, struct rte_hash_bucket *bkt, unsigned i)
{
	unsigned lcore_id, n_slots;
	struct lcore_cache *cached_free_slots;
    int keyid;
	bkt->sig_current[i] = NULL_SIGNATURE;
	bkt->sig_alt[i] = NULL_SIGNATURE;
	if (h->hw_trans_mem_support) {
		lcore_id = rte_lcore_id();
		cached_free_slots = &h->local_free_slots[lcore_id];
		/* Cache full, need to free it. */
		if (cached_free_slots->len == LCORE_CACHE_SIZE) {
			/* Need to enqueue the free slots in global ring. */
		//	n_slots = rte_ring_mp_enqueue_burst(h->free_slots,
		//				cached_free_slots->objs,
		//				LCORE_CACHE_SIZE, NULL);
            n_slots = 0;
            for(keyid = 0; keyid<LCORE_CACHE_SIZE ;keyid++)
            {
                if(cirq_add(h->free_slots,cached_free_slots->objs[n_slots]) == SUCCESS)
                {
					n_slots++;
                }
            }
			cached_free_slots->len -= n_slots;
		}
		/* Put index of new free slot in cache. */
		cached_free_slots->objs[cached_free_slots->len] =
				(void *)((uintptr_t)bkt->key_idx[i]);
		cached_free_slots->len++;
	} else {
		cirq_add(h->free_slots,(void *)((uintptr_t)bkt->key_idx[i]));
	//	rte_ring_sp_enqueue(h->free_slots,
	//			(void *)((uintptr_t)bkt->key_idx[i]));
	}
}

static inline int32_t
__qing_hash_del_key_with_hash(const struct qing_hash *h, const void *key,
						hash_sig_t sig)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	unsigned i;
	struct rte_hash_bucket *bkt;
	struct rte_hash_key *k, *keys = h->key_store;
	int32_t ret;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == sig &&
				bkt->key_idx[i] != EMPTY_SLOT) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				remove_entry(h, bkt, i);

				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				ret = bkt->key_idx[i] - 1;
				bkt->key_idx[i] = EMPTY_SLOT;
				return ret;
			}
		}
	}

	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == alt_hash &&
				bkt->key_idx[i] != EMPTY_SLOT) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				remove_entry(h, bkt, i);

				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				ret = bkt->key_idx[i] - 1;
				bkt->key_idx[i] = EMPTY_SLOT;
				return ret;
			}
		}
	}

	return -ENOENT;
}

int32_t
qing_hash_del_key(const struct qing_hash *h, const void *key)
{
	RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);

	hash_sig_t r;
    r = rte_hash_crc(key,h->key_len,h->hash_func_init_val);
	//return __qing_hash_del_key_with_hash(h, key, rte_hash_hash(h, key));
	return __qing_hash_del_key_with_hash(h, key, r);
}


static inline int32_t
__qing_hash_lookup_with_hash(const struct qing_hash *h, const void *key,
					hash_sig_t sig, void **data)
{
	uint32_t bucket_idx;
	hash_sig_t alt_hash;
	unsigned i;
	struct rte_hash_bucket *bkt;
	struct rte_hash_key *k, *keys = h->key_store;

	bucket_idx = sig & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in primary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == sig &&
				bkt->key_idx[i] != EMPTY_SLOT) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				if (data != NULL)
					*data = k->pdata;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

	/* Calculate secondary hash */
	alt_hash = rte_hash_secondary_hash(sig);
	bucket_idx = alt_hash & h->bucket_bitmask;
	bkt = &h->buckets[bucket_idx];

	/* Check if key is in secondary location */
	for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
		if (bkt->sig_current[i] == alt_hash &&
				bkt->sig_alt[i] == sig) {
			k = (struct rte_hash_key *) ((char *)keys +
					bkt->key_idx[i] * h->key_entry_size);
			if (rte_hash_cmp_eq(key, k->key, h) == 0) {
				if (data != NULL)
					*data = k->pdata;
				/*
				 * Return index where key is stored,
				 * subtracting the first dummy index
				 */
				return bkt->key_idx[i] - 1;
			}
		}
	}

	return -ENOENT;
}


int
qing_hash_lookup_data(const struct qing_hash *h, const void *key, void **data)
{
    RETURN_IF_TRUE(((h == NULL) || (key == NULL)), -EINVAL);

	hash_sig_t r;
    r = rte_hash_crc(key,h->key_len,h->hash_func_init_val);
	//return __qing_hash_lookup_with_hash(h, key, rte_hash_hash(h, key), data);
	return __qing_hash_lookup_with_hash(h, key, r, data);
}

