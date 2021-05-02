#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf usbredir
rm -f usbredir.tar.gz
rm -f ${TARGZ}/usbredir.tar.gz

git clone https://gitlab.freedesktop.org/spice/usbredir.git
cd ${HERE}/usbredir
./autogen.sh
rm -rf ./autom4te.cache
rm -rf ./.git
patch -p1 < ../usbredir.patch 
cd ${HERE} 
tar zcvf usbredir.tar.gz usbredir
rm -rf usbredir
mv usbredir.tar.gz ${TARGZ}

