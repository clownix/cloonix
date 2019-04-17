#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../../../targz_store
rm -rf spice-gtk
rm -f ${TARGZ}/spice-gtk.tar.gz
git clone https://gitlab.freedesktop.org/spice/spice-gtk.git
cd spice-gtk
sed -i s/--enable-gtk-doc/--disable-gtk-doc/ autogen.sh
sed -i s/--enable-vala/--disable-vala/ autogen.sh
cd spice-gtk
./autogen.sh
rm -rf ./autom4te.cache
rm -rf ./spice-common/autom4te.cache
rm -rf .git
rm -rf spice-common/.git
rm -f  gtk-doc.make 
rm -f  m4/gtk-doc.m4 
cd $HERE
tar zcvf spice-gtk.tar.gz spice-gtk
rm -rf spice-gtk
mv spice-gtk.tar.gz ${TARGZ}

