#!/bin/bash
HERE=`pwd`
TARGZSTORE=${HERE}/../../../targz_store
NAMEZ=dpdk.tar.gz
NAME=dpdk
rm -rf ${NAME}
tar xvf ${TARGZSTORE}/${NAMEZ}
cd ${HERE}/${NAME}
echo patch -p1 < ${HERE}/dpdk.patch
patch -p1 < ${HERE}/dpdk.patch
sleep 2
sed -i s'%-rpath=$(RTE_SDK_BIN)/lib%-rpath=/usr/local/bin/cloonix/server/dpdk/lib%' mk/rte.app.mk
echo "LDFLAGS+=\"-rpath=/usr/local/bin/cloonix/server/dpdk/lib\"" >> mk/rte.vars.mk
tar xvf ${TARGZSTORE}/meson.tar.gz
tar xvf ${TARGZSTORE}/ninja.tar.gz

cd ${HERE}/${NAME}/ninja
sed -i s"%/usr/bin/env python%/usr/bin/env python3%" configure.py
sed -i s"%/usr/bin/env python%/usr/bin/env python3%" bootstrap.py
./configure.py --bootstrap
cd ${HERE}/${NAME}
export PATH=${HERE}/${NAME}/ninja:$PATH
${HERE}/${NAME}/meson/meson.py build
cd build
cp rte_build_config.h ../config
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
rm -vrf ${HERE}/usr
sed -i s"%libdir=\${prefix}/lib64%libdir=${HERE}/lib%" ${HERE}/lib/pkgconfig/libdpdk.pc
sed -i s"%libdir=\${prefix}/lib/x86_64-linux-gnu%libdir=${HERE}/lib%" ${HERE}/lib/pkgconfig/libdpdk.pc
sed -i s"%includedir=\${prefix}/include%includedir=${HERE}/include%" ${HERE}/lib/pkgconfig/libdpdk.pc

