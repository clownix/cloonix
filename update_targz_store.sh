#!/bin/bash
set -xe
HERE=`pwd`
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store
rm -rf ${WORK}
mkdir -vp ${WORK}
#-----------------------------------------------------

#-----------------------------------------------------
for i in "websockify-js" "noVNC"; do
  cd ${WORK}
  git clone --depth=1 https://github.com/novnc/${i}.git
  cd ${WORK}/${i}
  COMMIT=$(git log --pretty=format:"%H")
  cd ${WORK}
  tar zcvf ${i}_${COMMIT}.tar.gz ${i}
  rm -rf ${i}
  mv ${i}_${COMMIT}.tar.gz ${TARGZ}
done
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/nginx/nginx.git
cd ${WORK}/nginx
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf nginx_${COMMIT}.tar.gz nginx
rm -rf nginx
mv nginx_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/NixOS/patchelf.git
cd ${WORK}/patchelf
COMMIT=$(git log --pretty=format:"%H")
./bootstrap.sh
./configure
cd ${WORK}
tar zcvf patchelf_${COMMIT}.tar.gz patchelf 
rm -rf patchelf
mv patchelf_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/containers/crun.git
cd ${WORK}/crun
COMMIT=$(git log --pretty=format:"%H")
./autogen.sh
./configure
cd ${WORK}
tar zcvf crun_${COMMIT}.tar.gz crun
rm -rf crun
mv crun_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

#-----------------------------------------------------
cd ${WORK}
git clone --depth=1 https://github.com/tmux/tmux.git
cd ${WORK}/tmux
COMMIT=$(git log --pretty=format:"%H")
sh autogen.sh
cd ${WORK}
tar zcvf tmux_${COMMIT}.tar.gz tmux
rm -rf tmux
mv tmux_${COMMIT}.tar.gz ${TARGZ}
#-----------------------------------------------------

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
meson subprojects download
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
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice-gtk.git
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
