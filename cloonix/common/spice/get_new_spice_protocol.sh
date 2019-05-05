#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../../targz_store
git clone https://gitlab.freedesktop.org/spice/spice-protocol.git
cd spice-protocol
./autogen.sh
rm -rf .git
rm -rf autom4te.cache
cd $HERE
tar zcvf spice-protocol.tar.gz spice-protocol
rm -rf spice-protocol
mv spice-protocol.tar.gz ${TARGZ}

