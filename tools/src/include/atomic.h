/**
 * @file atomic.h
 * @brief atomic processes. Don't include it!(build-in functions from gcc, 
 * refer to https://www.jianshu.com/p/cb7b726e943c)
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2018.6.27
 * @version 1.0
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2018.6.27
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __ATOMIC_H_
#define __ATOMIC_H_
/******************************************************************************/
//#if (GCC_VERSION >= 40100)
/* memory access barrier */
#define barrier()					(__sync_synchronize())
/* atomic get */
#define AO_GET(ptr)					({ __typeof__(*(ptr)) volatile *_val = \
				(ptr);barrier(); (*_val); })
/* atomic set if value is different old value*/
#define AO_SET(ptr, value)			((void)\
				__sync_lock_test_and_set((ptr), (value)))
/* atomic swap, return old value if success, otherwise return setting value*/
#define AO_SWAP(ptr, value)			((__typeof__(*(ptr)))\
				__sync_lock_test_and_set((ptr), (value)))
/* atomic CAS, set new value and return old value if old value not changed,
 * otherwise return new value */
#define AO_CAS(ptr, comp, value)	((__typeof__(*(ptr)))\
				__sync_val_compare_and_swap((ptr), (comp), (value)))
/* atomic CAS, set new value and return true if old value net changed,
 * otherwise return false */
#define AO_CASB(ptr, comp, value)	(__sync_bool_compare_and_swap\
				((ptr), (comp), (value)) != 0 ? true : false)
/* atomic clear */
#define AO_CLEAR(ptr)				((void)__sync_lock_release((ptr)))
/*----------------------------------------------------------------------------*/
/* operations to old value with returning new value*/
#define AO_ADD_F(ptr, value)		((__typeof__(*(ptr)))\
				__sync_add_and_fetch((ptr), (value)))
#define AO_SUB_F(ptr, value)		((__typeof__(*(ptr)))\
				__sync_sub_and_fetch((ptr), (value)))
#define AO_OR_F(ptr, value)			((__typeof__(*(ptr)))\
				__sync_or_and_fetch((ptr), (value)))
#define AO_AND_F(ptr, value)		((__typeof__(*(ptr)))\
				__sync_and_and_fetch((ptr), (value)))
#define AO_XOR_F(ptr, value)		((__typeof__(*(ptr)))\
				__sync_xor_and_fetch((ptr), (value)))
/*----------------------------------------------------------------------------*/
/* operations to old value with returning old value*/
#define AO_F_ADD(ptr, value)		((__typeof__(*(ptr)))\
				__sync_fetch_and_add((ptr), (value)))
#define AO_F_SUB(ptr, value)		((__typeof__(*(ptr)))\
				__sync_fetch_and_sub((ptr), (value)))
#define AO_F_OR(ptr, value)			((__typeof__(*(ptr)))\
				__sync_fetch_and_or((ptr), (value)))
#define AO_F_AND(ptr, value)		((__typeof__(*(ptr)))\
				__sync_fetch_and_and((ptr), (value)))
#define AO_F_XOR(ptr, value)		((__typeof__(*(ptr)))\
				__sync_fetch_and_xor((ptr), (value)))

/* operations without return */
#define AO_INC(ptr)                 ((void)AO_ADD_F((ptr), 1))
#define AO_DEC(ptr)                 ((void)AO_SUB_F((ptr), 1))
#define AO_ADD(ptr, val)            ((void)AO_ADD_F((ptr), (val)))
#define AO_SUB(ptr, val)            ((void)AO_SUB_F((ptr), (val)))
#define AO_OR(ptr, val)          ((void)AO_OR_F((ptr), (val)))
#define AO_AND(ptr, val)            ((void)AO_AND_F((ptr), (val)))
#define AO_XOR(ptr, val)            ((void)AO_XOR_F((ptr), (val)))
/* bit set 1 with returning new value */
#define AO_BIT_ON(ptr, mask)        AO_OR_F((ptr), (mask))
/* bit set 0 with returning new value */
#define AO_BIT_OFF(ptr, mask)       AO_AND_F((ptr), ~(mask))
/* bit flipping with returning new value */
#define AO_BIT_XCHG(ptr, mask)      AO_XOR_F((ptr), (mask))

#endif //#ifdef __ATOMIC_H_
