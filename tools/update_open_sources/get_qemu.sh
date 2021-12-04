#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf qemu*
rm -f ${TARGZ}/qemu_*.tar.gz
# 
git clone --depth=1 https://git.qemu.org/git/qemu.git
cd ${HERE}/qemu
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
./scripts/archive-source.sh ${HERE}/qemu.tar.gz
cd ${HERE}
mkdir qemu_vip
cd ${HERE}/qemu_vip
tar xvf ${HERE}/qemu.tar.gz
cd ${HERE}
tar zcvf qemu_${COMMIT}.tar.gz qemu_vip
rm -rf qemu
rm -rf qemu_vip
rm -f qemu.tar.gz
mv qemu_${COMMIT}.tar.gz ${TARGZ}
