#!/bin/bash
HERE=`pwd`
TARGZSTORE=${HERE}/../../../targz_store
MESON_NINJA=${HERE}/../../common/meson_ninja
NAME=dpdk
NAMEZ=${NAME}.tar.gz
rm -rf ${NAME}
tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${HERE}/${NAME}
echo patch -p1 < ${HERE}/dpdk.patch
patch -p1 < ${HERE}/dpdk.patch
sleep 2
sed -i s'%-rpath=$(RTE_SDK_BIN)/lib%-rpath=/usr/local/bin/cloonix/server/dpdk/lib%' mk/rte.app.mk
echo "LDFLAGS+=\"-rpath=/usr/local/bin/cloonix/server/dpdk/lib\"" >> mk/rte.vars.mk
export PATH=${MESON_NINJA}/ninja:$PATH

BASE=${HERE}/${NAME}/config/common_base
sed -i s"/CONFIG_RTE_PROC_INFO=n/CONFIG_RTE_PROC_INFO=y/" ${BASE}

${MESON_NINJA}/meson/meson.py -Dexamples=kni build
cd ${HERE}/${NAME}/build
ninja
DESTDIR=${HERE} ninja install

mv -v ${HERE}/usr/local/bin ${HERE}
mv -v ${HERE}/usr/local/include ${HERE}
if [ -d ${HERE}/usr/local/lib64 ]; then
  mv -v ${HERE}/usr/local/lib64 ${HERE}
  mv -v ${HERE}/lib/modules ${HERE}/lib64
  rmdir -v ${HERE}/lib
  mv -v ${HERE}/lib64 ${HERE}/lib
else
  mv -v ${HERE}/usr/local/lib/x86_64-linux-gnu ${HERE}
  mv -v ${HERE}/lib/modules ${HERE}/x86_64-linux-gnu
  rmdir -v ${HERE}/lib
  mv -v ${HERE}/x86_64-linux-gnu ${HERE}/lib
fi
cp -f ${HERE}/${NAME}/build/examples/dpdk-kni ${HERE}/bin
sed -i s"%libdir=\${prefix}/lib64%libdir=${HERE}/lib%" ${HERE}/lib/pkgconfig/libdpdk.pc
sed -i s"%libdir=\${prefix}/lib/x86_64-linux-gnu%libdir=${HERE}/lib%" ${HERE}/lib/pkgconfig/libdpdk.pc
sed -i s"%includedir=\${prefix}/include%includedir=${HERE}/include%" ${HERE}/lib/pkgconfig/libdpdk.pc

