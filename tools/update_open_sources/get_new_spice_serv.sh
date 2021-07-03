#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf spice*
rm -f ${TARGZ}/spice_*.tar.gz
git clone --depth=1 https://gitlab.freedesktop.org/spice/spice.git
cd  ${HERE}/spice
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
./autogen.sh
sed -i s/^run_command/#run_command/ meson.build
sed -i s/run_command\(.*\)/\'0.14.3.180-851b-dirty\'/ meson.build
cd ${HERE}
tar zcvf spice_${COMMIT}.tar.gz spice
rm -rf spice
mv spice_${COMMIT}.tar.gz ${TARGZ} 

