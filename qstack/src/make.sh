QINGYUN_HOME=$(cd `dirname $0`; pwd)/../..
QSTACK_HOME=$QINGYUN_HOME/qstack
QMEM_PATH=$QINGYUN_HOME/tools/mempool
DPDK_PATH=$QINGYUN_HOME/dpdk-lnk

if [ ! -d "$DPDK_PATH" ];then
    echo "$DPDK_PATH is needed! Please read the README"
    exit
else
    DPDK_INCLUDE=$DPDK_PATH/include
    DPDK_LIB=$DPDK_PATH/lib
    if [ ! -h "$DPDK_INCLUDE" -o ! -h "$DPDK_LIB" ];then
        echo "$DPDK_INCLUDE or $DPDK_LIB is not linked correctly. Please read the README"
        exit
    fi
fi

if [ "$1" = "dpdk" ]; then
    cd $QINGYUN_HOME/dpdk/usertools
    ./dpdk-setup.sh
fi

cd $QINGYUN_HOME/tools/qcoroutine/src
make clean
make -j20
cd $QINGYUN_HOME/tools/mempool/src
make clean
make -j20
cd $QINGYUN_HOME/qstack/src
make clean
make -j20
cd $QINGYUN_HOME/apps
make clean
sudo make -j20
