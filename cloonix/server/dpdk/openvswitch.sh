#!/bin/bash
HERE=`pwd`
TARGZSTORE=../../../targz_store
NAMEZ=openvswitch-2.11.1.tar.gz
NAME=openvswitch-2.11.1

tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${NAME}
sed -i s"/DPDK_RING_SIZE = 256/DPDK_RING_SIZE = 128/" lib/netdev-dpdk.c
./boot.sh
export DPDK_CFLAGS=-I./include
export DPDK_LDFLAGS=-L./lib
export LDFLAGS="-Wl,-rpath -Wl,/usr/local/bin/cloonix/server/dpdk/lib"
./configure --prefix=${HERE} --with-dpdk=${HERE} --disable-ssl
make -j 6
make install

