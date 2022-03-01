#ifndef CONNECT_H
#define CONNECT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <hiredis/hiredis.h>


typedef struct parseArgv{
    char * ip;
    redisContext * context;
    int port;
    int start_slot;
    int end_slot;
    char slots[16384];
}parseArgv;

typedef struct clusterInfo{
    int len;
    char * argv[50];
    parseArgv* parse[50];
    void * slot_to_host[16384];
    redisContext* globalContext;
}clusterInfo;


int __set_nodb(redisContext* cluster,const char* key,const char* set_in_value);
int __set_withdb(redisContext* cluster,const char* key, const char* set_in_value, int dbnum, int core_id);


int __get_withdb(redisContext*cluster, const char* key,char*get_in_value,int dbnum, int core_id);
int __get_nodb(redisContext*cluster, const char* key,char* get_in_value);

void __process_cluster_str(char* str);
clusterInfo* __clusterInfo(redisContext* localContext);

void print_clusterInfo_parsed(clusterInfo* mycluster);
void process_cluterInfo(clusterInfo* mycluster);

void from_str_to_cluster(char * temp, clusterInfo* mycluster);
void __test_slot(clusterInfo* mycluster);
void assign_slot(clusterInfo* mycluster);
void __add_context_to_cluster(clusterInfo* mycluster);
void __remove_context_from_cluster(clusterInfo* mycluster);
void __global_disconnect(clusterInfo* cluster);

void __set_redirect(char* str);

redisContext* __connect_cluster(char* ip, int port, int sock);

int flushDb(clusterInfo* cluster);

void init_global();
#endif
