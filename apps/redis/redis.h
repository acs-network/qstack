#ifndef REDIS_H
#define REDIS_H

#include "connect.h"

#define USER_DEVICE_DB	1
#define USER_KEY_DB		2
#define	DEVICE_STATE_DB	3

#define KEY_LEN			80
#define VALUE_LEN		80


redisContext* connectRedis();
void disconnectDatabase(clusterInfo* cluster);
int get(redisContext* cluster, const char *key, char *get_in_value, const int dbnum, int core_id);
int set(redisContext* cluster, const char *key, const char *set_in_value, const int dbnum, int core_id);
void flushDatabase(clusterInfo * cluster);

void redis_init_global();
#endif
