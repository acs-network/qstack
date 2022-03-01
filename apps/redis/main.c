#include<stdio.h>
#include"connect.h"
#include<pthread.h>
/*
*"value" is a char* type global variable which is used to hold value.
*It can be used in either get or set operation.
*/
void *db_function(void* input){
  char * ip = "172.16.32.208";
  int port = 7000;
  printf("thread=%d\n",*(int*)input);

  /*
  *each thread has its own value and cluster struct.
  */
  char*  value = (char*)malloc(1024*8); 
  clusterInfo *cluster = connectRedis(ip,port);
  if(cluster!=NULL){
  	//printf("!=null\n");
  }
  else printf("==null\n");

  int sum = 0;
  char* key = (char*)malloc(100);

  sprintf(key,"key=%d",*((int*)input));

  sum += set(cluster,key,"aaaaa",1);
  sum += get(cluster,key,value,1);
  printf("get %s: %s\n",key,value);

  sum += set(cluster,key,"fffff",1);
  sum += get(cluster,key,value,1);
  printf("get %s:%s\n",key,value);

  sum += set(cluster,key,"eafde",1);
  sum += get(cluster,key,value,1);
  printf("get %s: %s\n",key,value);

  sum += set(cluster,key,"abcde",1);
  sum += get(cluster,key,value,1);
  printf("get %s: %s\n",key,value);
  
  if(sum == 0)
     printf("all operations succeed!\n");
  else printf("operation fail\n");

  //flushDb(cluster);
  disconnectDatabase(cluster);
  printf("intput = %d\n",*((int*)input));
 
}
int main(){
 pthread_t th[16];
 int res, i;
 void* thread_result;
 int thread[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

 for(i=0;i<16;i++){ 
   res = pthread_create(&th[i],NULL,db_function,(void*)(&thread[i]));
   if(res!=0){
      printf("thread fail\n");
      break;
   }
  }

 for(i=0;i<16;i++){ 
   res = pthread_join(th[i],&thread_result);
   if(res!=0){
      printf("thread join fail\n");
      break;
   }
  }



 return 0;
}
