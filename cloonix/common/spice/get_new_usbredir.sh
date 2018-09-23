#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../../targz_store
rm -rf usbredir
rm -f usbredir.tar.gz
git clone git://anongit.freedesktop.org/spice/usbredir
cd usbredir
./autogen.sh
cd $HERE
rm -rf ./usbredir/autom4te.cache
rm -rf ./usbredir/.git
tar zcvf usbredir.tar.gz usbredir
rm -rf usbredir
mv usbredir.tar.gz ${TARGZ}

