#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store

rm -rf virglrenderer
rm -f virglrenderer.tar.gz
rm -f ${TARGZ}/virglrenderer.tar.gz
rm -rf libepoxy
rm -f libepoxy.tar.gz
rm -f ${TARGZ}/libepoxy.tar.gz

git clone --depth=1 https://github.com/anholt/libepoxy.git
git clone --depth=1 https://github.com/freedesktop/virglrenderer.git

tar zcvf virglrenderer.tar.gz virglrenderer 
tar zcvf libepoxy.tar.gz libepoxy
rm -rf virglrenderer
rm -rf libepoxy
mv virglrenderer.tar.gz ${TARGZ}
mv libepoxy.tar.gz ${TARGZ}

