#!/bin/bash
HERE=`pwd`
TARGZSTORE=../../../targz_store
NAMEZ=dpdk-19.02.tar.xz
NAME=dpdk-19.02
tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${NAME}
patch -p1 < ${HERE}/dpdk.patch
sed -i s'%-rpath=$(RTE_SDK_BIN)/lib%-rpath=/usr/local/bin/cloonix/server/dpdk/lib%' mk/rte.app.mk
echo "LDFLAGS+=\"-rpath=/usr/local/bin/cloonix/server/dpdk/lib\"" >> mk/rte.vars.mk
echo "LDLIBS  += -lrte_pmd_tap" >> app/test-pmd/Makefile
echo "LDLIBS  += -lrte_pmd_ring" >> app/test-pmd/Makefile
echo "LDLIBS  += -lrte_pmd_tap" >> app/proc-info/Makefile
echo "LDLIBS  += -lrte_pmd_ring" >> app/proc-info/Makefile

sed -i s/CONFIG_RTE_PROC_INFO=n/CONFIG_RTE_PROC_INFO=y/ \
       config/common_base
sed -i s/CONFIG_RTE_BUILD_SHARED_LIB=n/CONFIG_RTE_BUILD_SHARED_LIB=y/ \
       config/common_base
sed -i s/CONFIG_RTE_LIBRTE_PMD_SOFTNIC=n/CONFIG_RTE_LIBRTE_PMD_SOFTNIC=y/ \
       config/common_base
sed -i s/CONFIG_RTE_LIBRTE_PMD_TAP=n/CONFIG_RTE_LIBRTE_PMD_TAP=y/ \
       config/common_base

sed -i s/CONFIG_RTE_EAL_VFIO=y/CONFIG_RTE_EAL_VFIO=n/ \
       config/common_linuxapp
sed -i s/CONFIG_RTE_KNI_KMOD=y/CONFIG_RTE_KNI_KMOD=n/ \
       config/common_linuxapp
sed -i s/CONFIG_RTE_LIBRTE_KNI=y/CONFIG_RTE_LIBRTE_KNI=n/ \
       config/common_linuxapp
sed -i s/CONFIG_RTE_LIBRTE_PMD_KNI=y/CONFIG_RTE_LIBRTE_PMD_KNI=n/ \
       config/common_linuxapp


make config T=x86_64-native-linuxapp-gcc
make -j 6 install T=x86_64-native-linuxapp-gcc DESTDIR=${HERE}
cp ./usertools/dpdk-devbind.py ${HERE}/bin

