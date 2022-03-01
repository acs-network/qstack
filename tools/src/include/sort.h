/**
 * @file sort.h 
 * @brief quick sort function tool 
 * @author Shen Yifan (shenyifan@ict.ac.cn)
 * @date 2020.7.25
 * @version 1.0 
 * @detail Function list: \n
 *   1. sort():quick sort for an array\n
 *   2. \n
 */
/*----------------------------------------------------------------------------*/
/* - History:
 *   1. Date: 2020.7.25 
 *   	Author: Shen Yifan
 *   	Modification: create
 *   2. Date:
 *   	Author:
 *   	Modification:
 */
/******************************************************************************/
#ifndef __SORT_H_
#define __SORT_H_
/******************************************************************************/
#include <stdio.h>
#include <stdint.h>
/******************************************************************************/

#if 0
typedef uint64_t SORT_TYPE;

int
uint64_gt(void *a, void *b)
{ 
	return *((SORT_TYPE*)a) > *((SORT_TYPE*)b);
}
#endif
// use greater function if sort with descending order
typedef int(FUNC_CMP)(SORT_TYPE *a, SORT_TYPE *b);

/******************************************************************************/
static inline void
get_mid(SORT_TYPE a[], int l, int r, FUNC_CMP cmp_func)
{
	int m = (l + r)/2;
	int i;

	if (cmp_func(&a[l], &a[m])) {
		// l<m
		if (cmp_func(&a[m], &a[r])) {
			// l<m<r
			i = m;
		} else {
			// r<m
			if (cmp_func(&a[l], &a[r])) {
				// l<r<m
				i = r;
			} else {
				// r<l<m
				i = l;
			}
		}
	} else {
		// m<l
		if (cmp_func(&a[r], &a[m])) {
			// r<m<l
			i = m;
		} else {
			// m<r
			if (cmp_func(&a[r], &a[l])) {
				// m<r<l
				i = r;
			} else {
				// m<l<r
				i = l;
			}
		}
	}
	if (i == l ) {
		return;
	}
	SORT_TYPE temp = a[i];
	a[i] = a[l];
	a[l] = temp;
}

/**
 * sort the array
 *
 * @param a			target array
 * @param left		the start offset of the array
 * @param right		the end offset of the array
 * @param cmp_func	the compare function for sorting. If sort with descending 
 * 					order, return 1 if a>b.
 *
 * @return null
 *
 * @note
 * 	define SORT_TYPE and cmp_func before using sort() 
 */
static void
sort(SORT_TYPE a[], int left, int right, FUNC_CMP cmp_func)
{
	if (left >= right) {
		return;
	}

	int i = left;
	int j = right;
	get_mid(a, left, right, cmp_func);
	SORT_TYPE key = a[left];	// make a hole at left
	
	while (i < j) {
		while (i<j && !cmp_func(&a[j], &key)) {
			j--;
		}
		if (i < j) {
			a[i++] = a[j];
		}
		while (i<j && cmp_func(&a[i], &key)) {
			i++;
		}
		if (i < j) {
			a[j--] = a[i];
		}
	}
	a[i] = key; // i==j, pointing to the hole

	sort(a, left, i - 1, cmp_func);
	sort(a, i + 1, right, cmp_func);
}
/******************************************************************************/
#endif //#ifdef __SORT_H_
