#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice-gtk*
rm -rf wayland*
rm -f ${TARGZ}/spice-gtk_*.tar.gz
rm -f ${TARGZ}/wayland_*.tar.gz
git clone --depth=1 https://github.com/wayland-project/wayland.git
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice-gtk.git
cd ${HERE}/spice-gtk
COMMITGTK=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
cd ${HERE}/wayland
COMMITWAY=$(git log --pretty=format:"%H")
cd ${HERE}
tar zcvf wayland_${COMMITWAY}.tar.gz wayland
tar zcvf spice-gtk_${COMMITGTK}.tar.gz spice-gtk
rm -rf wayland
rm -rf spice-gtk
mv wayland_${COMMITWAY}.tar.gz ${TARGZ}
mv spice-gtk_${COMMITGTK}.tar.gz ${TARGZ}


