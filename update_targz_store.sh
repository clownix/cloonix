#!/bin/bash
set -e
HERE=`pwd`
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store

rm -rf ${WORK}
mkdir -vp ${WORK}

#-----------------------------------------------------
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
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/openvswitch/ovs.git
cd ${WORK}/ovs
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf ovs_${COMMIT}.tar.gz ovs
rm -rf ovs
mv ovs_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/freedesktop/spice-gtk.git
cd ${WORK}/spice-gtk
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
cd ${WORK}
tar zcvf spice-gtk_${COMMIT}.tar.gz spice-gtk
rm -rf spice-gtk
mv spice-gtk_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice-protocol.git
cd spice-protocol
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf spice-protocol_${COMMIT}.tar.gz spice-protocol
rm -rf spice-protocol
mv spice-protocol_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice.git
cd spice
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
cd ${WORK}
tar zcvf spice_${COMMIT}.tar.gz spice
rm -rf spice
mv spice_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://gitlab.freedesktop.org/spice/usbredir.git
cd usbredir
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf usbredir_${COMMIT}.tar.gz usbredir 
rm -rf usbredir
mv usbredir_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

cd ${HERE}
rmdir work_targz_store
