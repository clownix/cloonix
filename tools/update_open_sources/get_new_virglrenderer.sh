#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf virglrenderer
rm -f virglrenderer.tar.gz
rm -f ${TARGZ}/virglrenderer.tar.gz

git clone https://github.com/freedesktop/virglrenderer.git
cd virglrenderer
./autogen.sh
cd $HERE
rm -rf ./virglrenderer/autom4te.cache
rm -rf ./virglrenderer/.git
tar zcvf virglrenderer.tar.gz virglrenderer 
rm -rf virglrenderer
mv virglrenderer.tar.gz ${TARGZ}

