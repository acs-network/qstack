.PHONY: clean

### TARGET ###
TARGETS=runme
QCORO_LIB_DIR=../lib
QCORO_LIB=libqcoro.a
QCORO_HDR_DIR=../include
QCORO_HDR = routine.h

# headers of universal tools
QTOOL_FLD	= ../../tools
QTOOL_INC	= -I${QTOOL_FLD}/src/include

# MEMEPOOL LIBRARY and HEADER
QMEM_FLD    =../../mempool
QMEM_INC    =-I${QMEM_FLD}/src/include
QMEM_LIB    =-L${QMEM_FLD}/lib

# qstack library and header 
QSTACK_FLD    =../../qstack
QSTACK_INC    =-I${QSTACK_FLD}/src/include
QSTACK_LIB    =-L${QSTACK_FLD}/lib

### GCC ###
GCC=gcc

GCC_OPT = -w -g -O3 -m64 
#GCC_OPT += -Wall -fPIC -fgnu89-inline -z muldefs 
#GCC_OPT += $(DBG_OPT)

### LIBRARIES AND INCLUDES ###
INC_DIR=./include
INC= -I$(INC_DIR) ${QSTACK_INC} ${QTOOL_INC} ${QMEM_INC}

# DPDK LIBRARY and HEADER
DPDK_INC=../../dpdk-lnk/include
DPDK_LIB=../../dpdk-lnk/lib
#TODO: zhangzhao
DPDK_MACHINE_FLAGS = $(shell cat ../../dpdk-lnk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h

### SOURCE CODE ###
SRCS = $(wildcard *.c)

OBJS = $(patsubst %.c,%.o,$(SRCS))
DEPS = $(patsubst %.c,.%.d,$(SRCS))
COBJS = coctx_swap.o
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

default: $(COBJS) $(QCORO_HDR)
	mkdir -p $(QCORO_LIB_DIR)
	ar rvs $(QCORO_LIB_DIR)/$(QCORO_LIB) $(COBJS)

$(OBJS): %.o: %.c Makefile
	$(MSG) "   CC $<"
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -c $< -o $@
$(DEPS): .%.d: %.c Makefile
#	$(HIDE) 
	$(GCC) $(GCC_OPT) $(INC) -MM $(CFLAGS) $< > $@

-include $(DEPS)

$(QCORO_HDR):
	mkdir -p $(QCORO_HDR_DIR)
	cp $(INC_DIR)/$@ $(QCORO_HDR_DIR)/$@

clean: clean-library
	rm -f *.o *~ core
	rm -f .*.d

clean-library:
	rm -f $(QCORO_LIB_DIR)/*
	rm -f $(QCORO_HDR_DIR)/*

distclean: clean
	rm -f Makefile
