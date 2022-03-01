#include "connect.h"
#include <stdio.h>
#include <errno.h>
#include "crc16.h"
#include <string.h>

//#define DEBUG
//#define SUCCESS

/*
*use the globalContext to connect to the host specified by the user;
*send cluster nodes command through this context;
*receive the infomation and create clusterInfo struct,
*which keeps connections to all nodes in the cluster.
*/
redisContext* __connect_cluster(char* ip, int port, int sock){


     //globalGetKey = (char*)malloc(1024);
/*     
     if(value == NULL||globalSetKey==NULL){
       printf("can not assign global value space, no connection either\n");
       return;
     }*/
	redisContext* localContext;
    if(sock > 0){
		char path[50];
		sprintf(path, "/run/redis_%d.sock", sock);
		localContext = redisConnectUnix(path);
	 } else if(port > 0){
     	localContext = redisConnect(ip,port);
	 }else{
		localContext->err = 1;
		perror("no redis sock or port connect!\n");
	 }
	
	 if(localContext->err){
	     redisFree(localContext);
		 //printf("global connection refused\n");
		 perror("global connection refused\n");
		 return;
	 }else{
#ifdef DEBUG	 
	     printf("succeed in global connecting\n");
		 return localContext;
#endif
	 }
}


/*
*once we meet indirection, this function help parse the information returned
*/
void __set_redirect(char* str){
	char *slot,*ip,*port;
	slot = strchr(str,' ');
	slot++;
	ip = strchr(slot,' ');
	ip++;
	port = strchr(ip,':');
	port++;
	//s means store
	char* s_slot, *s_ip, *s_port;
        s_slot = (char*)malloc(ip-slot);
	strncpy(s_slot,slot,ip-slot-1);
	s_slot[ip-slot-1]='\0';

	s_ip = (char*)malloc(port-ip);
	strncpy(s_ip,ip,port-ip-1);
	s_ip[port-ip-1]='\0';

	s_port = (char*)malloc(sizeof(port)+1);
	strcpy(s_port,port);
#ifdef DEBUG
	printf("get new ip = %s slot=%s port =%s\n",s_ip,s_slot,s_port);
#endif
	free(s_ip);
	free(s_slot);
	free(s_port);
}

/*
*calculate the slot, find the context, and then send command
*/
int __set_nodb(redisContext* c,const char* key,const char* set_in_value){
	redisReply *r = (redisReply *)redisCommand(c, "set %s %s", key, set_in_value);

	if (r->type == REDIS_REPLY_STRING){

		printf("set should not return str ?value = %s\n", r->str);
		freeReplyObject(r);
		return 0;
	}else if(r->type == REDIS_REPLY_ERROR && !strncmp(r->str,"MOVED",5)){
		printf("set still need redirection ? %s\n", r->str);
		__set_redirect(r->str);
		freeReplyObject(r);
		return -1;
	}else if(r->type == REDIS_REPLY_STATUS){
#ifdef DEBUG
		printf("STATUS = %s\n",r->str);
#endif
		freeReplyObject(r);
		return 0;
	}else{
	   printf("set error\n");
	   freeReplyObject(r);
	   return -1;
	}
}

/*
*set method with the db option
*/


int __set_withdb(redisContext* cluster,const char* key, const char* set_in_value, int dbnum, int core_id){
    //char* localSetKey = (char*)malloc(1024);
    char localSetKey[35];
    //memset(localSetKey, 0, 1024);
    sprintf(localSetKey,"%d\b%s",dbnum,key);
	int re = __set_nodb(cluster,localSetKey,set_in_value);
    //free(localSetKey);   
	return re;
}


/*
*get method without use db option. here const char* is not compitable with char*
*/
int __get_nodb(redisContext* c ,const char* key,char* get_in_value){
        assert(get_in_value != NULL);
	if(key==NULL){
	   strcpy(get_in_value,"key is NULL");
	   return -1;
	}

	redisReply *r = (redisReply *)redisCommand(c, "get %s", key);
#ifdef DEBUG
	printf("type: %d\n", r->type);
#endif
	if (r->type == REDIS_REPLY_STRING) {
#ifdef DEBUG
		printf("%s = %s\n", key, r->str);
#endif  
		int len = strlen(r->str);
		strcpy(get_in_value, r->str);
		freeReplyObject(r);
		return 0;
	}else if (r->type == REDIS_REPLY_NIL) {
		strcpy(get_in_value,"nil");
		freeReplyObject(r);
		return 0;
	} else if(r->type == REDIS_REPLY_ERROR && !strncmp(r->str,"MOVED",5)){
		freeReplyObject(r);
#ifdef DEBUG
		printf("get still need redirection  %s\n",r->str);
#endif
		strcpy(get_in_value,"redirection");
		return -1;
	} else {
		printf("unknowd type return get\n");
		return -1;
	}
}



int __get_withdb(redisContext* cluster, const char* key, char* get_in_value,int dbnum, int core_id){
	//char *localGetKey = (char*)malloc(1024);
    char localGetKey[35];
    //memset(localGetKey, 0, 1024);        
	sprintf(localGetKey,"%d\b%s",dbnum,key);
	int re = __get_nodb(cluster,localGetKey,get_in_value);
	//free(localGetKey);
	return re;
}



/*
*This function uses the globle context to send cluster nodes command, and build a clusterInfo
*based on the string returned. It does this by calling functions from_str_to_cluster,
*process_clusterInfo,and addign_slot;
*/
clusterInfo* __clusterInfo(redisContext* localContext){
    redisContext *c = localContext;
    redisReply* r = (redisReply*)redisCommand(c,"cluster nodes");
#ifdef DEBUG
    printf("the raw return value = %s\n",r->str);
#endif
    clusterInfo* mycluster = (clusterInfo*)malloc(sizeof(clusterInfo));
    mycluster->globalContext = localContext;
    from_str_to_cluster(r->str,mycluster);
    process_cluterInfo(mycluster);
#ifdef DEBUG
    print_clusterInfo_parsed(mycluster);
#endif
    assign_slot(mycluster);
#ifdef DEBUG
    __test_slot(mycluster);
#endif
    __add_context_to_cluster(mycluster);
    return mycluster;
}

/*
*this function should be called after from_str_to_cluster.It parses the string for each node,and 
*store the information in mycluster->parse[i]. the parse fild are pointers to  parseArgv struct,each
*node in the cluster has exactly one such struct, which contains infomation such as ip,port,slot,context..
*/
void process_cluterInfo(clusterInfo* mycluster){
    //determine the time of iteration
    int len = mycluster->len;
#ifdef DEBUG
    printf("len = %d\n",mycluster->len);
#endif
    int len_ip=0;
    int len_port=0;
    char* temp;
    char* ip_start, *port_start, *port_end,*connect_start,*slot_start,*slot_end;

    char* temp_slot_start;
    char* temp_port;

    int i=0;
    for(;i<len;i++){
        temp = ip_start = port_start = port_end = connect_start = slot_start = slot_end = temp_slot_start = temp_port = NULL;

        temp = mycluster->argv[i];

        //parse ip
        ip_start = strchr(temp,' ');
        temp = ip_start+1;
        port_start = strchr(temp,':');
        temp = port_start+1;
        port_end = strchr(temp,' ');
        ip_start++;
        len_ip = port_start - ip_start;
        mycluster->parse[i] = (parseArgv*)malloc(sizeof(parseArgv));//do not forget
        mycluster->parse[i]->ip = (char*)malloc(len_ip + 1);

        strncpy(mycluster->parse[i]->ip,ip_start,len_ip);
        mycluster->parse[i]->ip[len_ip]='\0';

        //parsePort
        port_start++;
        len_port = port_end - port_start;
        temp_port = (char*)malloc(len_port+1);
        strncpy(temp_port,port_start,len_port);
        temp_port[len_port]='\0';
        mycluster->parse[i]->port = atoi(temp_port);

        connect_start = strstr(port_end,"connected");
        connect_start = strchr(connect_start,' ');

        slot_start = connect_start+1;
        slot_end = strchr(slot_start,'-');
        slot_end++;

        temp_slot_start = (char*)malloc(slot_end-slot_start);
        strncpy(temp_slot_start,slot_start,slot_end-slot_start-1);
        temp_slot_start[slot_end-slot_start-1]='\0';

        mycluster->parse[i]->start_slot = atoi(temp_slot_start);
        mycluster->parse[i]->end_slot = atoi(slot_end);

        free(temp_slot_start);
    }
}
/*
*print all the information about the cluster fro debuging.
*/
void print_clusterInfo_parsed(clusterInfo* mycluster){
    int len = mycluster->len;
    int i;
    for(i=0;i<len;i++){
        printf("node %d info\n",i);
        printf("original command: %s\n",mycluster->argv[i]);
        printf("ip: %s, port: %d\n",mycluster->parse[i]->ip,mycluster->parse[i]->port);
        printf("slot_start= %d, slot_end = %d\n",mycluster->parse[i]->start_slot,\
	             mycluster->parse[i]->end_slot);
    }
}

/*
*command cluster nodes will return a str, which fall into n parts, one for each node in the 
*cluster. this function add the strs to argv in clusterInfo struct, and set mycluster->len, which
*is the number of nodes in the cluster.
*/
void from_str_to_cluster(char * temp, clusterInfo* mycluster){
    char * argv[50];
    int count = 0;
    char* point;
    int copy_len = 0;

    while((point=strchr(temp,'\n'))!=NULL){
        copy_len = point-temp+1;
        argv[count] = (char*)malloc(copy_len);
        strncpy(argv[count],temp,copy_len - 1);

	if(strstr(argv[count],"slave")!=NULL){
	     free(argv[count]);
	     temp = point+1;
#ifdef DEBUG
	     printf("meet slave");
#endif
	     continue;
	}
        argv[count][copy_len-1] = '\0';
        count++;
        temp = point+1;
    }
    argv[count] = temp;
    int i;
    for(i = 0;i<count;i++){
        mycluster->argv[i] = argv[i];
    }
    mycluster->len = count;
}

void __test_slot(clusterInfo* mycluster){
    int slot[] = {0,5460,5461,10922,10923,16383};
    int len = sizeof(slot) / sizeof(int);
    int i=0;
    for(i=0;i<len;i++){
        printf("slot = %d info: ip = %s, port = %d.\n",slot[i],((parseArgv*)mycluster->slot_to_host[slot[i]])->ip, ((parseArgv*)mycluster->slot_to_host[slot[i]])->port);
    }
}


void assign_slot(clusterInfo* mycluster){
    int len = mycluster->len;
    int i;
    int count = 0;
    for(i=0;i<len;i++){
        int start = mycluster->parse[i]->start_slot;
        int end = mycluster->parse[i]->end_slot;
        int j=0;
	if(sizeof(mycluster->parse[i]->slots)!=16384){
	     printf("slot != 16384 in assign_slot\n");
	     return;
	} 
	memset(mycluster->parse[i]->slots,0,16384);

        for(j=start;j<=end;j++){
            mycluster->slot_to_host[j] = (void*)(mycluster->parse[i]);
            count++;
	    mycluster->parse[i]->slots[i]=1;
        }
    }
#ifdef DEBUG
    printf("count = %d \n",count);
#endif
}

/*
*This function give each node in the cluster a context connection
*/
void __add_context_to_cluster(clusterInfo* mycluster){
   int len = mycluster-> len;
   int i = 0;
   redisContext * tempContext;
   
   for(i=0;i<len;i++){
       tempContext = redisConnect((mycluster->parse[i])->ip,(mycluster->parse[i])->port);
       if(tempContext->err){
			printf("connection refused in __add_contect_to_cluster\n");
			printf("refuse ip=%s, port=%d\n",(mycluster->parse[i])->ip,(mycluster->parse[i])->port);
	  redisFree(tempContext);
	  return;
       }else{
          (mycluster->parse[i])->context = tempContext;
       }
   }
#ifdef SUCCESS
   printf("%d connections established!!\n",len);
#endif
}


void __remove_context_from_cluster(clusterInfo* mycluster){
   int len = mycluster-> len;
   int i = 0;
   redisContext * tempContext;
   
   for(i=0;i<len;i++){
      if(mycluster->parse[i]->context != NULL)      
           redisFree(mycluster->parse[i]->context);
      else printf("context == NULL in remove_context_from_cluster\n");
   }
#ifdef SUCCESS
   printf("%d connections closed!!\n",len);
#endif 
}



/*
*The global context is used to inquery cluster information
*/
void __global_disconnect(clusterInfo* cluster){
     if(cluster->globalContext !=NULL)
     redisFree(cluster->globalContext);
#ifdef SUCCESS
     printf("global context freed\n");
#endif
}

/*
*Flushdb command. 0 means the command succeed, otherwise fail.
*/
int flushDb(clusterInfo* cluster){
    redisContext *c = NULL;
    if(cluster==NULL){
    	printf("error flushdb cluster == NULL\n");
    }

    int len = cluster->len;
    int i;

    for(i=0;i<len;i++){
       c = cluster->parse[i]->context;
       redisReply *r = (redisReply *)redisCommand(c, "flushdb");
       if(r->type == REDIS_REPLY_STATUS){

           printf("flushdb status = %s\n",r->str);

	}else if(r->type == REDIS_REPLY_STRING){
           printf("flush db = %s\n",r->str);
        }else{
	   break;
        }
    }
    if(i == len) return 0;
    else return -1;
}

  void init_global() {
  }
