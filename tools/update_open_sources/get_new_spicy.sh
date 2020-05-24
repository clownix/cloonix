#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice-gtk
rm -f spice-gtk.tar.gz
rm -f ${TARGZ}/spice-gtk.tar.gz
git clone --depth=1 https://github.com/wayland-project/wayland.git
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice-gtk.git
cd spice-gtk
git submodule init
git submodule update --recursive
cd ${HERE}
tar zcvf wayland.tar.gz wayland
tar zcvf spice-gtk.tar.gz spice-gtk
rm -rf wayland
rm -rf spice-gtk
mv wayland.tar.gz ${TARGZ}
mv spice-gtk.tar.gz ${TARGZ}


