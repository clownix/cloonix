#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice-protocol*
rm -f ${TARGZ}/spice-protocol_*.tar.gz 

git clone --depth=1 https://gitlab.freedesktop.org/spice/spice-protocol.git
cd spice-protocol
COMMIT=$(git log --pretty=format:"%H")
./autogen.sh
rm -rf .git
rm -rf autom4te.cache
cd $HERE
tar zcvf spice-protocol_${COMMIT}.tar.gz spice-protocol
rm -rf spice-protocol
mv spice-protocol_${COMMIT}.tar.gz ${TARGZ}

