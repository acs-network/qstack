HOME=$(shell env | grep HOME | cut -d "=" -f 2)
ifeq ($(HOME),/root)
 $(error Do not compile as root)
endif

CC=gcc
CXX=g++
GCC_OPT = -m64 -Wall -fPIC -c
GCC_OPT += -DNDEBUG -O3 -DNETSTAT -DINFO -DDBGERR -DDBGCERR


### LIBRARIES AND INCLUDES ###
CFLAGS = -I.


### COMPILE ###
all:
	${CC} ${GCC_OPT} ${CFLAGS} crc16.c
	${CC} ${GCC_OPT} ${CFLAGS} connect.c
	${CC} ${GCC_OPT} ${CFLAGS} redis.c

clean:
	 rm -rf *~ *.o

