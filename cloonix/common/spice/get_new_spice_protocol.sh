#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../../targz_store
git clone git://git.freedesktop.org/git/spice/spice-protocol
cd spice-protocol
./autogen.sh
rm -rf .git
rm -rf autom4te.cache
cd $HERE
tar zcvf spice-protocol.tar.gz spice-protocol
rm -rf spice-protocol
mv spice-protocol.tar.gz ${TARGZ}

