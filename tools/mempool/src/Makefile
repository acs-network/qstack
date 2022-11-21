.PHONY: clean

HOME=$(shell env | grep HOME | cut -d "=" -f 2)
ifeq ($(HOME),/root)
 $(error Do not compile as root)
endif

### TARGET ###
TARGETS=runme
QMEM_LIB_DIR=../lib
QMEM_LIB=libqmempool.a
QMEM_HDR_DIR=../include
QMEM_HDR = dpdk_chunk.h dpdk_mempool.h

# headers of universal tools
QTOOL_FLD	= ../../../tools
QTOOL_INC	= -I${QTOOL_FLD}/src/include

# qstack library and header 
QSTACK_FLD    =../../../qstack
QSTACK_INC    =-I${QSTACK_FLD}/src/include
QSTACK_LIB    =-L${QSTACK_FLD}/lib

#zz
### GCC ###
GCC=gcc

GCC_OPT = -w -g -O3 -m64 
#GCC_OPT += -Wall -fPIC -fgnu89-inline -z muldefs 
#GCC_OPT += $(DBG_OPT)

### LIBRARIES AND INCLUDES ###
INC_DIR=./include
INC= -I$(INC_DIR) ${QSTACK_INC} ${QTOOL_INC}

# DPDK LIBRARY and HEADER
DPDK_INC=../../../dpdk-lnk/include
DPDK_LIB=../../../dpdk-lnk/lib
#TODO: zhangzhao
DPDK_MACHINE_FLAGS = $(shell cat ../../../dpdk-lnk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h

### SOURCE CODE ###
SRCS = dpdk_mempool.c dpdk_chunk.c \

OBJS = $(patsubst %.c,%.o,$(SRCS))
DEPS = $(patsubst %.c,.%.d,$(SRCS))
#COBJS = mempool_swap.o
COBJS += $(OBJS)

ifeq ($V,) # no echo
	export MSG=@echo
	export HIDE=@
else
	export MSG=@\#
	export HIDE=
endif

### GOALS ###
all: default

default: $(COBJS) $(QMEM_HDR)
	mkdir -p $(QMEM_LIB_DIR)
	ar rvs $(QMEM_LIB_DIR)/$(QMEM_LIB) $(COBJS)

$(OBJS): %.o: %.c Makefile
	$(MSG) "   CC $<"
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -c $< -o $@
$(DEPS): .%.d: %.c Makefile
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -MM $(CFLAGS) $< > $@

-include $(DEPS)

$(QMEM_HDR):
	mkdir -p $(QMEM_HDR_DIR)
	cp $(INC_DIR)/$@ $(QMEM_HDR_DIR)/$@

clean: clean-library
	rm -f *.o *~ core
	rm -f .*.d

clean-library:
	rm -f $(QMEM_LIB_DIR)/*
	rm -f $(QMEM_HDR_DIR)/*

distclean: clean
	rm -f Makefile
