#!/bin/bash
set -e
HERE=`pwd`
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store

rm -rf ${WORK}
mkdir -vp ${WORK}
cd ${WORK}

git clone --depth=1 https://git.qemu.org/git/qemu.git
cd ${WORK}/qemu
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
./scripts/archive-source.sh ${WORK}/qemu.tar.gz
cd ${WORK}
mkdir qemu_vip
cd ${WORK}/qemu_vip
tar xvf ${WORK}/qemu.tar.gz
cd ${WORK}
tar zcvf qemu_${COMMIT}.tar.gz qemu_vip
rm -rf qemu
rm -rf qemu_vip
rm -f qemu.tar.gz
mv qemu_${COMMIT}.tar.gz ${TARGZ}


cd ${WORK}
git clone --depth=1 https://github.com/openvswitch/ovs.git
git clone --depth=1 https://github.com/mesonbuild/meson.git
git clone --depth=1 https://github.com/ninja-build/ninja.git

for i in ovs meson ninja; do
  cd ${WORK}/${i}
  COMMIT=$(git log --pretty=format:"%H")
  cd ${WORK}
  tar zcvf ${i}_${COMMIT}.tar.gz ${i}
  rm -rf ${i}
  mv ${i}_${COMMIT}.tar.gz ${TARGZ}
done

cd ${WORK}
git clone --depth=1 https://github.com/freedesktop/spice-gtk.git
cd ${WORK}/spice-gtk
COMMITGTK=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive

cd ${WORK}
tar zcvf spice-gtk_${COMMITGTK}.tar.gz spice-gtk
rm -rf spice-gtk
mv spice-gtk_${COMMITGTK}.tar.gz ${TARGZ}


cd ${HERE}
rmdir work_targz_store
