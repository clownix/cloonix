#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice-gtk*
rm -f ${TARGZ}/spice-gtk_*.tar.gz
git clone --depth=1 https://github.com/freedesktop/spice-gtk.git
cd ${HERE}/spice-gtk
COMMITGTK=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
cd ${HERE}
tar zcvf spice-gtk_${COMMITGTK}.tar.gz spice-gtk
rm -rf spice-gtk
mv spice-gtk_${COMMITGTK}.tar.gz ${TARGZ}


