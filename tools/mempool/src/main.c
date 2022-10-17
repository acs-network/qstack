/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <assert.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include "./include/dpdk_mempool.h"
#include "./include/dpdk_chunk.h"
//#define TEST_1 //alloc from public mempool
//#define TEST_2 //alloc from private mempool and public mempool 
//#define TEST_3 //alloc and free random
#define ALLOC_FREE_THREAD 16
#define MAX_THREAD ALLOC_FREE_THREAD + 1
//tacek zhangzhao
#define TYPE 2
#define data_type_0  rte_mbuf
#define DATA_TY(TYPE) data_type_##TYPE
//#define DATA_STRUCT DATA_TY(TYPE)
#define DATA_STRUCT data_type_2
#define LOOPTIMES 1000000
volatile struct Mempool_t *mempool_base = NULL;
static struct DATA_STRUCT * data[400000];
FILE * file_fd[40];
int mark[200000];


/*=========================*/
//this test loop 100000000 times alloc chunk from mempool 
//only alloc from public pool.we free chunk space back to pool
//randomly free chunk
/*=========================*/
void test_4(int lcore_id,FILE *file_t)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int m = 0;
	int nu = 0;
	int fn = 0;
   	struct tcp_stream *p;
#define MAX_ARRAY 1000
    struct tcp_stream *array[MAX_ARRAY];
	int mark[MAX_ARRAY];

	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};
	int randnum = 0;
	fprintf(file_t,"begin test 1 in core id %d \n ",lcore_id);
	clock_gettime(CLOCK_REALTIME, &ts_start);
	for(i = 0; i < MAX_ARRAY;i++){
		array[i] = NULL;
		mark[i] = 0;
	}


    for(i = 0; i < LOOPTIMES;){
		j = rand()%MAX_ARRAY;
    	p = mempool_alloc_chunk(mempool_base,lcore_id);

		if(p != NULL){
			i++;
			if(i < 100000){
				continue;//i < 100000 means it from private pool
			}
			while(mark[j] != 0){
					j = (j + 1 )%MAX_ARRAY;
			}
			array[j] = p;
			mark[j] = 1;
			i++;
			rte_mb();
		}else{
			nu++;
		}

		for(k = 0; k < 2 ;k++){
				fn = rand()%MAX_ARRAY;
				if(array[fn] != NULL){
				//if(array[fn] != NULL && mark[fn] != 0){
					mempool_free_chunk(mempool_base,array[fn],lcore_id);
					array[fn] = NULL;
					mark[fn] = 0;
				}
		}
	}
	clock_gettime(CLOCK_REALTIME, &ts_end);

	fprintf(file_t,"core id is %d p->id is NULL times is %d and NULL rate is %d /100 \n ",lcore_id,nu,nu*100/(LOOPTIMES+nu));
	fprintf(file_t,"alloc and free  %ld times and each time eed  %ld ns in core %d\n ", \
			LOOPTIMES,(1000000000/LOOPTIMES)*(ts_end.tv_sec - ts_start.tv_sec)  + (ts_end.tv_nsec - ts_start.tv_nsec)/LOOPTIMES,lcore_id);
}

/*=========================*/
//this test loop 100000000 times alloc chunk from mempool 
//only alloc from private pool.we free chunk space back to pool
//randomly free chunk
/*=========================*/
void test_3(int lcore_id,FILE *file_t)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int m = 0;
	int nu = 0;
	int fn = 0;
   	struct DATA_STRUCT *p;
#define MAX_ARRAY 2000
    struct DATA_STRUCT *array[MAX_ARRAY];
	int mark[MAX_ARRAY];

	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};
	int randnum = 0;
	fprintf(file_t,"begin test 1 in core id %d \n ",lcore_id);
	clock_gettime(CLOCK_REALTIME, &ts_start);
	for(i = 0; i < MAX_ARRAY;i++){
		array[i] = NULL;
		mark[i] = 0;
	}


    for(i = 0; i < LOOPTIMES;){
		j = rand()%MAX_ARRAY;
    	p = mempool_alloc_chunk(mempool_base,lcore_id);
		if(p != NULL){
//       	fprintf(file_t,"core id is %d p->id is NULL and nu is %d \n ",lcore_id,nu);
			while(mark[j] != 0){
	//				j = rand()%MAX_ARRAY;
					j = (j + 1 )%MAX_ARRAY;
	//				printf("test 1 and j is %d \n",j);
			}

    //   		printf("core id is %d p->id is %d and nu is %d and j is %d and i is %d \n ",lcore_id,p->id,nu,j,i);
       		//fprintf(file_t,"core id is %d p->id is %d and nu is %d and j is %d \n ",lcore_id,p->id,nu,j);
			array[j] = p;
			mark[j] = 1;
			i++;
				k = 0;
				for(k = 0; k < 1 ;k++){
						fn = rand()%MAX_ARRAY;
						if(array[fn] != NULL){
							mempool_free_chunk(mempool_base,array[fn],lcore_id);
							array[fn] = NULL;
							mark[fn] = 0;
						}
				}

				rte_mb();
		}else{
			nu++;
       		fprintf(file_t,"core id is %d p->id is NULL and nu is %d \n ",lcore_id,nu);
		//	assert(k > 0);
		}
	}

	clock_gettime(CLOCK_REALTIME, &ts_end);

	fprintf(file_t,"core id is %d p->id is NULL times is %d and NULL rate is %f%100 \n ",lcore_id,nu,nu*100/(LOOPTIMES+nu));
	fprintf(file_t,"alloc and free  %ld times and each time eed  %ld ns in core %d\n ", \
			LOOPTIMES,(1000000000/LOOPTIMES)*(ts_end.tv_sec - ts_start.tv_sec)  + (ts_end.tv_nsec - ts_start.tv_nsec)/LOOPTIMES,lcore_id);
}


/*=========================*/
//this test loop 100000000 times alloc chunk from mempool 
//only alloc from public pool.we free chunk space back to pool
/*=========================*/
void test_1(int lcore_id,FILE *file_t)
{
	
	int i = 0;
	int j = 0;
	int k = 0;
//   	struct DATA_STRUCT *p;
	struct tcp_stream *p;
    void *array[32];
	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};

	fprintf(file_t,"begin test 1 in core id %d \n ",lcore_id);
	clock_gettime(CLOCK_REALTIME, &ts_start);
    for(i = 0; i < LOOPTIMES;){
    p = mempool_alloc_chunk(mempool_base,lcore_id);
		if(p != NULL){
			array[j] = p;
			printf("j is %d and p is 0x%x \n",j,p);
			//   	struct DATA_STRUCT *p;
#if 1
			if(i < 100000){
//				if(p->id != i){
//					fprintf(file_t,"p ->id is %d and i is %d \n",p->id ,i);
//				}
//				assert(p->id == i);
//				assert(p->pool ==  mempool_base->mempool_p[lcore_id]);
				if(p->pool !=  mempool_base->mempool_p[lcore_id]){\
              		fprintf(file_t,"core id is %d p->id is %d and i is %d and pool name is %s\n ",lcore_id,p->id,i,p->pool->name);
				}

			//	assert(p->pool ==  mempool_base->mempool_p[lcore_id]);
			}
			if(i > 100100){
				//assert(p->pool ==  mempool_base->mempool_s);
			}
#endif
					
			i++;
			j = j + 1;
			if(j == 32){
				j = 0;
				k = 0;
				for(k = 0; k < 32; k++){
		//			if(i > 100000){
						printf("k is %d and array[k] is 0x%x \n",k,array[k]);
						mempool_free_chunk(mempool_base,array[k],lcore_id);
		//			}
				}			
			}		
		}else{
       		fprintf(file_t,"core id is %d p->id is NULL\n ",lcore_id);
			assert(k > 0);
		}
	}

	clock_gettime(CLOCK_REALTIME, &ts_end);

	fprintf(file_t,"alloc and free  %ld times and each time eed  %ld ns in core %d\n ", \
			LOOPTIMES,(1000000000/LOOPTIMES)*(ts_end.tv_sec - ts_start.tv_sec)  + (ts_end.tv_nsec - ts_start.tv_nsec)/LOOPTIMES,lcore_id);
}


/*=========================*/
//this test loop 100000000 times alloc chunk from mempool 
//after we get the chunk space .we free it back to pool
//so we can only get chunk space from private pool
/*=========================*/
void test_2(int lcore_id,FILE *file_t)
{
	int i = 0;
	int j = 0;
	int k = 0;
   	struct DATA_STRUCT *p;
    struct DATA_STRUCT *array[32];
	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};

	clock_gettime(CLOCK_REALTIME, &ts_start);
	fprintf(file_t,"begin test 2 in core id %d \n ",lcore_id);
    for(i = 0; i < LOOPTIMES;){
    p = mempool_alloc_chunk(mempool_base,lcore_id);
		if(p != NULL){
			array[j] = p;
#if 1
			if(i < 100000){
			//	assert(p->id == i);
			//	assert(p->pool ==  mempool_base->mempool_p[lcore_id]);
			}
			if(i > 100100){
			//	assert(p->pool ==  mempool_base->mempool_p[lcore_id]);
	//			assert(p->pool ==  mempool_base->mempool_s);
			}
#endif
					
			i++;
			j = j + 1;
			if(j == 32){
				j = 0;
				k = 0;
				for(k = 0; k < 32; k++){
						mempool_free_chunk(mempool_base,array[k],lcore_id);
				}			
			}		
		}else{
       		fprintf(file_t,"core id is %d p->id is NULL\n ",lcore_id);
			k = -1;
			assert(k > 0);
		}
	}

	clock_gettime(CLOCK_REALTIME, &ts_end);

//	fprintf(file_t,"core id is %d p->id is NULL times is %d and NULL rate is %f%100 \n ",lcore_id,nu,nu*100/(LOOPTIMES+nu));
	fprintf(file_t,"alloc and free  %ld times and each time eed  %ld ns in core %d\n ", \
			LOOPTIMES,(1000000000/LOOPTIMES)*(ts_end.tv_sec - ts_start.tv_sec)  + (ts_end.tv_nsec - ts_start.tv_nsec)/LOOPTIMES,lcore_id);
//	fprintf(file_t,"alloc and free  %ld times and each time eed  %ld ns in core %d\n ", \
			LOOPTIMES,(1000000000/LOOPTIMES)*(ts_end.tv_sec - ts_start.tv_sec)  + (ts_end.tv_nsec - ts_start.tv_nsec)/LOOPTIMES,lcore_id);
} 




static int
lcore_hello(__attribute__((unused)) void *arg)
{	 
//struct Mempool_t *mempool_create(int type, uint32_t p_pool_size ,uint32_t s_pool_size, int core_num){
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
   	struct DATA_STRUCT *p;
    struct DATA_STRUCT *array[32];
	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};

	//printf("hello from core %u mempool_base->type is %d \n", lcore_id,mempool_base->type);
    sleep(1);
	int time = 0;
	int i = 0;
    int j = 0;
    int k = 0;
	int mark_c = 0;
	int test_c = 0;

    if(lcore_id > MAX_THREAD){
        return 0;
    }
	char mz_name[RTE_MEMZONE_NAMESIZE];
	snprintf(mz_name, 10, "log_%d", lcore_id);
	printf("mz_name is %s \n",mz_name);
	file_fd[lcore_id] = fopen(mz_name,"wr");
	if(file_fd[lcore_id] != NULL && lcore_id < MAX_THREAD){
	    printf("open success in core %d \n",lcore_id);
	}else{
		return -1;
	}
	
    if(lcore_id < MAX_THREAD){
		test_4(lcore_id,file_fd[lcore_id]);
#if 0
		clock_gettime(CLOCK_REALTIME, &ts_start);
   //     	for(i = 0; i < 100000 + 100000/(MAX_THREAD-1);){
		//	if(mempool_base->type != 0){
		//	}
	//		while(1){
        	for(i = 0; i < 10000000;){
	//			if(mempool_base->type != 0){
	//				mempool_base->type = 0;
	//			}
           		p = mempool_alloc_chunk(mempool_base,lcore_id);
           		if(p != NULL){
           //   		fprintf(file_fd[lcore_id],"core id is %d p->id is %d and i is %d and pool name is %s\n ",lcore_id,p->id,i,p->pool->name);
					array[j] = p;
					//this is test 1
#if 0
					if(i < 100000){
						assert(p->id == i);
						assert(p->pool ==  mempool_base->mempool_p[lcore_id]);
					}
					if(i > 100100){
						assert(p->pool ==  mempool_base->mempool_s);
					}
#endif

//					mempool_free_chunk(mempool_base,array[k]);
					//test 1 over
					//this is test 2
#if 0
					if(i >= 200000){
						mark_c = 1;
					}
					if(mark_c == 1){
						//alloc from public pool
						assert(p->pool ==  mempool_base->mempool_s);
						assert(mark[p->id] == 0);
						mark[p->id] = 1;

						test_c++;
					}
#endif
					//test 2 over
					
					i++;
					j = j + 1;
					if(j == 32){
					j = 0;
					k = 0;
					for(k = 0; k < 32; k++){
						if(i >= 8100){
							mempool_free_chunk(mempool_base,array[k]);
						}
					}			
				}		
			}else{

                		fprintf(file_fd[lcore_id],"core id is %d p->id is NULL\n ",lcore_id);
			}
     //   	if(i%1000000 == 0){	
	//			clock_gettime(CLOCK_REALTIME, &ts_end);
    //            fprintf(file_fd[lcore_id],"alloc and free 1000000 times and each time eed  %d ns in core %d\n ",(ts_end.tv_sec - ts_start.tv_sec) * 1000 + (ts_end.tv_nsec - ts_start.tv_nsec)/1000000,lcore_id);
	//		}
		}
		clock_gettime(CLOCK_REALTIME, &ts_end);

                fprintf(file_fd[lcore_id],"alloc and free 10000000 times and each time eed  %d ns in core %d\n ",(ts_end.tv_sec - ts_start.tv_sec) * 100 + (ts_end.tv_nsec - ts_start.tv_nsec)/10000000,lcore_id);
            //    fprintf(file_fd[lcore_id],"alloc and free 10000000 times need  %d s and  %d ns in core %d\n ",ts_end.tv_sec - ts_start.tv_sec,ts_end.tv_nsec - ts_start.tv_nsec,lcore_id);
#endif
	}
	sleep(2);
	int sum = 0;
	for(i = 0;i<200000;i++){
		sum = mark[i] + sum; 
	}
	printf("core id is %d test_c is %d and sum is %d \n",lcore_id,test_c,sum);
	return 0;
}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	lcore_id = 0;
	struct timespec ts_end = {0,0};
 	struct timespec ts_start = {0,0};
	printf("first test for malloc and free \n");
#if 0	
	clock_gettime(CLOCK_REALTIME, &ts_start);
	int n = 0;
	struct DATA_STRUCT *te = NULL;
	struct DATA_STRUCT *be = NULL;
	struct DATA_STRUCT *he = NULL;
	for(n = 0;n < 10000000;){
		te = (struct DATA_STRUCT*)malloc(sizeof(struct DATA_STRUCT));
		if(te == NULL){
		}else{
			n++;
			if(be != NULL){
				be->next = te;
			}
			if(be == NULL){
				he = te;
			}
			be = te;
		}
	}
	te = he;
	for(n = 0;n<1000000;n++){
		assert(te != NULL);
		be = te->next;
		free(te);
		te = be;
	}
	
	clock_gettime(CLOCK_REALTIME, &ts_end);

    printf("alloc and free 10000000 times and each time eed  %d ns in core %d\n ",(ts_end.tv_sec - ts_start.tv_sec) * 100 + (ts_end.tv_nsec - ts_start.tv_nsec)/10000000,lcore_id);


#endif
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	struct Mempool_t *mempool_t = NULL;
	lcore_id = rte_lcore_id();
	mempool_t = mempool_create(TYPE,2000000,1000000,MAX_THREAD);
    if(mempool_t == NULL){
       		printf("alloc mempool_t fail \n");
		return NULL;
    }
    mempool_base = mempool_t;
	printf("mempool_base is 0x%x for our test \n",mempool_base);

	//printf("size of data type 0 is %d \n",RTE_MBUF_SIZE);
	printf("size of data type 1 is %d \n",TYPE_SIZE_1);
	printf("size of data type 2 is %d \n",TYPE_SIZE_2);
	printf("size of data type 3 is %d \n",TYPE_SIZE_3);
	printf("size of data type 4 is %d \n",TYPE_SIZE_4);

	int i = 0;
	for(i = 0;i< 200000;i++){
		mark[i] = 0;
	}
	/* call lcore_hello() on every slave lcore */
#if 1
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
	}
#endif
//	lcore_hello(NULL);
	/* call it on master lcore too */
//	lcore_hello(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
