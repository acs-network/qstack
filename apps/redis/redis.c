#include "redis.h"

//#define DEBUG

char *database_ip = "127.0.0.1";//"192.168.0.3";
int database_port = 6379;

redisContext* connectRedis(int port, int sock)
{
    return __connect_cluster(database_ip, port, sock);
}

void disconnectDatabase(clusterInfo* cluster)
{
    __global_disconnect(cluster);
    __remove_context_from_cluster(cluster);
	flushDb(cluster);
}

void flushDatabase(clusterInfo * cluster) 
{
	flushDb(cluster);
}

int get(redisContext* cluster, const char *key, char *get_in_value, const int dbnum, int core_id)
{
      return  __get_withdb(cluster, key, get_in_value, dbnum, core_id);
}

int set(redisContext* cluster, const char *key, const char *set_in_value, const int dbnum, int core_id)
{
	return __set_withdb(cluster, key, set_in_value, dbnum, core_id);
}

void redis_init_global(){
    init_global();
}
