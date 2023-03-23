# TODO: Make this Makefile.in pretty

TARGETS = echo_server iotepserver
#test_pkt
CC=gcc -g -O3 -DNDEBUG -lssl -lcrypto 

REDIS = 1

# DPDK LIBRARY and HEADER
DPDK_INC=../dpdk-lnk/include
DPDK_LIB=../dpdk-lnk/lib

# headers of universal tools
QTOOL_FLD	= ../tools
QTOOL_INC	= -I${QTOOL_FLD}/src/include

# qstack library and header 
QSTACK_FLD    =../qstack
QSTACK_INC    =-I${QSTACK_FLD}/src/include
QSTACK_LIB    =-L${QSTACK_FLD}/lib

QCORO_FLD    =../tools/qcoroutine
QCORO_INC    =-I${QCORO_FLD}/src/include
QCORO_LIB    =-L${QCORO_FLD}/lib


QMEM_FLD    =../tools/mempool
QMEM_INC    =-I${QMEM_FLD}/src/include
QMEM_LIB    =-L${QMEM_FLD}/lib

ifeq ($(REDIS),1)
REDIS_INC = -I./redis/
REDIS_FLD = ./redis
REDIS_OBJ = $(REDIS_FLD)/connect.o $(REDIS_FLD)/redis.o $(REDIS_FLD)/crc16.o
endif

INC = -I./include/ ${QSTACK_INC} 
LIBS = ${QSTACK_LIB} ${QCORO_LIB} ${QMEM_LIB}

# CFLAGS for DPDK-related compilation
INC += ${QTOOL_INC}
INC += ${QCORO_INC}
INC += ${QMEM_INC}

 
#CFLAGS += -D INLINE_DISABLED

#TODO: zhangzhao
DPDK_MACHINE_FLAGS = $(shell cat ../dpdk-lnk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h

ifeq ($(shell uname -m),x86_64)
LIBS += -m64
endif

DPDKLIB = -L../dpdk-lnk/lib/

DPDK_LIB_FLAGS = $(shell cat ../dpdk-lnk/lib/ldflags.txt)
INC += -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_FLD}/lib/libmtcp.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}
#LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${QSTACK_FLD}/lib/libqstack.a -L../../dpdk/lib -Wl,-lnuma -Wl,-lqstack -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,${DPDK_LIB_FLAGS}

#LIBS += -pthread -lrt -march=native -export-dynamic ${QSTACK_FLD}/lib/libqstack.a ${QCORO_LIB}/libqcoro.a ${QMEM_LIB}/libqmempool.a -L../dpdk-lnk/lib -lnuma -lqstack -lqcoro -lqmempool -lpthread -lrt -ldl ${DPDK_LIB_FLAGS}
LIBS += -pthread -lrt -march=native -export-dynamic ${QSTACK_FLD}/lib/libqstack.a ${QCORO_LIB}/libqcoro.a ${QMEM_LIB}/libqmempool.a -L../dpdk-lnk/lib -lnuma -lqstack -lqcoro -lqmempool -lpthread -lrt -ldl ${DPDK_LIB_FLAGS} 

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

all: ${TARGETS}
	mkdir -p build
	mv ${TARGETS} ./build/
	mv *.o ./build/

iotepserver: iotepserver.o 
	$(MSG) "   LD $<"
#	$(HIDE) 
	${CC} $< ${LIBS} -lhiredis ${REDIS_OBJ} -o $@

iotepserver.o: iotepserver.c ${QSTACK_FLD}/lib/libqstack.a 
	$(MSG) "   CC $<"
#	$(HIDE) 
	${CC} -c $< ${CFLAGS} ${INC}

echo_server: echo_server.o 
	$(MSG) "   LD $<"
#	$(HIDE) 
	${CC} $< ${LIBS} -o $@

echo_server.o: echo_server.c ${QSTACK_FLD}/lib/libqstack.a 
	$(MSG) "   CC $<"
#	$(HIDE) 
	${CC} -c $< ${CFLAGS} ${INC} > $@

clean:
	rm -f *~ *.o ${TARGETS} log_* err tmp core.* *.out
	./clean.sh

distclean: clean
	rm -rf Makefile
