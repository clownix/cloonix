#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
for i in dpdk ovs meson ninja; do
  rm -rf ${i}
  rm -f ${i}_*.tar.gz
  rm -f ${TARGZ}/${i}_*.tar.gz
done

git clone --depth=1 https://github.com/openvswitch/ovs.git
git clone --depth=1 https://github.com/DPDK/dpdk.git
git clone --depth=1 https://github.com/mesonbuild/meson.git
git clone --depth=1 https://github.com/ninja-build/ninja.git

for i in dpdk ovs meson ninja; do
  cd ${HERE}/${i}
  COMMIT=$(git log --pretty=format:"%H")
  cd ${HERE}
  tar zcvf ${i}_${COMMIT}.tar.gz ${i}
  rm -rf ${i}
  mv ${i}_${COMMIT}.tar.gz ${TARGZ}
done
