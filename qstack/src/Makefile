.PHONY: clean

### TARGET ###
TARGETS=runme
QSTACK_LIB_DIR=../lib
QSTACK_LIB=libqstack.a
QSTACK_HDR_DIR=../include
QSTACK_HDR = qstack.h api.h io_module.h


### GCC ###
GCC=gcc

GCC_OPT = -w -g -O3 -m64 -DNDEBUG -lssl -lcrypto
#GCC_OPT += -Wall -fPIC -fgnu89-inline -z muldefs 
#GCC_OPT += $(DBG_OPT)

### LIBRARIES AND INCLUDES ###
INC_DIR=./include
INC= -I$(INC_DIR)

# headers of universal tools
QTOOL_FLD	= ../../tools
QTOOL_INC	= -I${QTOOL_FLD}/src/include

# MEMEPOOL LIBRARY and HEADER
QMEM_FLD    =../../mempool
QMEM_INC    =-I${QMEM_FLD}/src/include
QMEM_LIB    =-L${QMEM_FLD}/lib

# COROUTING LIBRARY and HEADER
QCORO_FLD    =../../qcoroutine
QCORO_INC    =-I${QCORO_FLD}/src/include
QCORO_LIB    =-L${QCORO_FLD}/lib

# DPDK LIBRARY and HEADER
DPDK_INC=../../dpdk-lnk/include
DPDK_LIB=../../dpdk-lnk/lib

#TODO: zhangzhao
DPDK_MACHINE_FLAGS = $(shell cat ../../dpdk-lnk/include/cflags.txt)

INC += ${QMEM_INC}
INC += ${QCORO_INC}
INC += ${QTOOL_INC}
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h

### SOURCE CODE ###
#SRCS = basic_clock.c timestamp.c message.c core.c tcp_stream.c socket.c \
#	   eth_in.c ip_in.c \
#       tcp_in.c tcp_out.c io_module.c ip_out.c eth_out.c  \
#	   hashtable.c tcp_recv_buff.c api.c qepoll.c tcp_send_buff.c \
#	   dpdk_module.c flow_group.c flow_filter.c runtime_mgt.c arp.c \
#	   ssl.c
##SRCS +=	qing_hash.c 

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))
DEPS = $(patsubst %.c,.%.d,$(SRCS))

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

### GOALS ###
all: default

default: $(OBJS) $(QSTACK_HDR)
	mkdir -p $(QSTACK_LIB_DIR)
	ar rvs $(QSTACK_LIB_DIR)/$(QSTACK_LIB) $(OBJS)

$(OBJS): %.o: %.c Makefile
	$(MSG) "   CC $<"
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -c $< -o $@
$(DEPS): .%.d: %.c Makefile
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -MM $(CFLAGS) $< > $@

-include $(DEPS)

$(QSTACK_HDR):
	mkdir -p $(QSTACK_HDR_DIR)
	cp $(INC_DIR)/$@ $(QSTACK_HDR_DIR)/$@

clean: clean-library
	rm -f *.o *~ core
	rm -f .*.d

clean-library:
	rm -f $(QSTACK_LIB_DIR)/*
	rm -f $(QSTACK_HDR_DIR)/*

distclean: clean
	rm -f Makefile
