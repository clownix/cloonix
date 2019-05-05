#!/bin/bash
HERE=`pwd`
TARGZSTORE=../../../targz_store
NAMEZ=ovs.tar.gz
NAME=ovs
rm -rf ${NAME}
tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${NAME}
patch -p1 < ${HERE}/ovs.patch
./boot.sh
export PKG_CONFIG_PATH=${HERE}/lib/pkgconfig

export DPDK_CFLAGS="-I${HERE}/include -mavx"
export CFLAGS="-mavx"
export DPDK_LIBS="$(pkg-config --libs --static libdpdk)"
export DPDK_INCLUDE=-I${HERE}/include
export LDFLAGS="-Wl,-rpath,/usr/local/bin/cloonix/server/dpdk/lib"


sed -i s"%-ldpdk%${DPDK_LIBS}%" ./configure
./configure --prefix=${HERE} --with-dpdk=${HERE} --disable-ssl


make -j 6
make install

