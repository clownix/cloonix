#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
for i in dpdk ovs meson ninja; do
  rm -rf ${i}
  rm -f ${i}.tar.gz
  rm -f ${TARGZ}/${i}.tar.gz
done

git clone --depth=1 https://github.com/openvswitch/ovs.git
git clone --depth=1 https://github.com/DPDK/dpdk.git
git clone --depth=1 https://github.com/mesonbuild/meson.git
git clone --depth=1 https://github.com/ninja-build/ninja.git

for i in dpdk ovs meson ninja; do
  tar zcvf ${i}.tar.gz ${i}
  rm -rf ${i}
  mv ${i}.tar.gz ${TARGZ}
done
