/**
 * @file bitmap.h
 * @brief simple bitmap for n21_queue
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2019.4.2
 * @version 1.0
 * @detail Function list: \n
 *   1. bitmap_init(): init the bitmap\n
 *   2. bitmap_set(): set the target bit with flag\n
 *   3. bitmap_get(): get the target bit in the bitmap
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2019.4.2
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __BITMAP_H_
#define __BITMAP_H_
/******************************************************************************/
#include "universal.h"
/******************************************************************************/
/* global macros */
#define BITMAP_MAX_NUM		64		// max items in the bitmap
/******************************************************************************/
/* forward declarations */
typedef volatile uint64_t bitmap;
typedef bitmap *bitmap_t;
/******************************************************************************/
/* function declarations */
/**
 * init the bitmap and set all bits to 0
 *
 * @param map	target bitmap
 *
 * @return null
 */
static inline void
bitmap_init(bitmap_t map)
{
	*map = 0;
}

/**
 * set the traget bit with flag
 *
 * @param map		target bitmap
 * @param index		bit index in the bitmap
 * @param flag		target bit value
 *
 * @return null
 */
static inline void
bitmap_set(bitmap_t map, uint8_t index, uint8_t flag)
{
	uint64_t i = 0x1;
	if (likely(index < BITMAP_MAX_NUM)) {
		if (flag) {	// set 1
			*map |= i << index;
		} else {	// set 0
			*map &= (i << index) -1;
		}
	} else {
		TRACE_ERR("bitmap_index is bigger than limit!\n");
	}
}

/**
 * get the target bit in the bitmap
 *
 * @param map		target bitmap
 * @param index		bit index in the bitmap
 *
 * @return
 * 	return the target bit value (0 or 1)
 */
static inline uint8_t
bitmap_get(bitmap_t map, uint8_t index)
{
	uint64_t i = 0x1;
	if (likely(index < BITMAP_MAX_NUM)) {
		return 0 != (*map & (i << index));
	} else {
		TRACE_ERR("bitmap_index is bigger than limit!\n");
	}
}
/******************************************************************************/
#endif //#ifdef __BITMAP_H_
