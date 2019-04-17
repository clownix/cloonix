#!/bin/bash
HERE=`pwd`
TARGZSTORE=../../../targz_store
NAMEZ=dpdk-19.02.tar.xz
NAME=dpdk-19.02

tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${NAME}
sed -i s/CONFIG_RTE_BUILD_SHARED_LIB=n/CONFIG_RTE_BUILD_SHARED_LIB=y/ \
       config/common_base
make config T=x86_64-native-linuxapp-gcc
make -j 6 install O=mybuild T=x86_64-native-linuxapp-gcc DESTDIR=${HERE}

