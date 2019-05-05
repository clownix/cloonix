#!/bin/bash
HERE=`pwd`
TARGZSTORE=../../../targz_store
NAMEZ=dpdk.tar.gz
NAME=dpdk
rm -rf ${NAME}
tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${NAME}
echo patch -p1 < ${HERE}/dpdk.patch
patch -p1 < ${HERE}/dpdk.patch
sed -i s'%-rpath=$(RTE_SDK_BIN)/lib%-rpath=/usr/local/bin/cloonix/server/dpdk/lib%' mk/rte.app.mk
echo "LDFLAGS+=\"-rpath=/usr/local/bin/cloonix/server/dpdk/lib\"" >> mk/rte.vars.mk
sed -i s/CONFIG_RTE_PROC_INFO=n/CONFIG_RTE_PROC_INFO=y/ \
       config/common_base
sed -i s/CONFIG_RTE_BUILD_SHARED_LIB=n/CONFIG_RTE_BUILD_SHARED_LIB=y/ \
       config/common_base
sed -i s/CONFIG_RTE_LIBRTE_PMD_SOFTNIC=n/CONFIG_RTE_LIBRTE_PMD_SOFTNIC=y/ \
       config/common_base
sed -i s/CONFIG_RTE_LIBRTE_PMD_TAP=n/CONFIG_RTE_LIBRTE_PMD_TAP=y/ \
       config/common_base
sed -i s/CONFIG_RTE_EAL_VFIO=y/CONFIG_RTE_EAL_VFIO=n/ \
       config/common_linux
sed -i s/CONFIG_RTE_KNI_KMOD=y/CONFIG_RTE_KNI_KMOD=n/ \
       config/common_linux
sed -i s/CONFIG_RTE_LIBRTE_KNI=y/CONFIG_RTE_LIBRTE_KNI=n/ \
       config/common_linux
sed -i s/CONFIG_RTE_LIBRTE_PMD_KNI=y/CONFIG_RTE_LIBRTE_PMD_KNI=n/ \
       config/common_linux

meson build
cd build
ninja
DESTDIR=${HERE} ninja install

mv -v ${HERE}/usr/local/bin ${HERE}
mv -v ${HERE}/usr/local/include ${HERE}
mv -v ${HERE}/usr/local/lib/x86_64-linux-gnu ${HERE}
mv -v ${HERE}/lib/modules ${HERE}/x86_64-linux-gnu
rmdir -v ${HERE}/lib
mv -v ${HERE}/x86_64-linux-gnu ${HERE}/lib
rm -vrf ${HERE}/usr
sed -i s"%libdir=\${prefix}/lib/x86_64-linux-gnu%libdir=${HERE}/lib%" ${HERE}/lib/pkgconfig/libdpdk.pc
sed -i s"%includedir=\${prefix}/include%includedir=${HERE}/include%" ${HERE}/lib/pkgconfig/libdpdk.pc

