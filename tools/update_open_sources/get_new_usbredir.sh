#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf usbredir*
rm -f ${TARGZ}/usbredir_*.tar.gz

git clone --depth=1 https://gitlab.freedesktop.org/spice/usbredir.git
cd ${HERE}/usbredir
COMMIT=$(git log --pretty=format:"%H")
./autogen.sh
rm -rf ./autom4te.cache
rm -rf ./.git
patch -p1 < ../usbredir.patch 
cd ${HERE} 
tar zcvf usbredir_${COMMIT}.tar.gz usbredir
rm -rf usbredir
mv usbredir_${COMMIT}.tar.gz ${TARGZ}

