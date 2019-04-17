#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../../targz_store
git clone https://gitlab.freedesktop.org/spice/spice-protocol.git
cd spice-protocol
git checkout f6e4cb65da7c9e6350c7e14ae1753757560aad51
./autogen.sh
rm -rf .git
rm -rf autom4te.cache
cd $HERE
tar zcvf spice-protocol.tar.gz spice-protocol
rm -rf spice-protocol
mv spice-protocol.tar.gz ${TARGZ}

