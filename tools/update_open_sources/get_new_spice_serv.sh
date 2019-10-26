#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice
rm -f spice/.tar.gz
rm -f ${TARGZ}/spice/.tar.gz

git clone https://gitlab.freedesktop.org/spice/spice.git
cd spice
./autogen.sh
cd $HERE
rm -rf ./spice/autom4te.cache
rm -rf ./spice/spice-common/autom4te.cache
rm -rf ./spice/.git
rm -rf ./spice/spice-common/.git
tar zcvf spice.tar.gz spice
rm -rf spice
mv spice.tar.gz ${TARGZ} 
