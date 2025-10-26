#!/bin/bash
HERE=`pwd`
DEST="/usr/libexec/cloonix/cloonfs/bin"
cd ${HERE}/../lib_io_clownix
make clean
make
cd ${HERE}
make clean
make
sudo mv ${HERE}/cloonix-suid-power ${DEST}
sudo chown root:root ${DEST}/cloonix-suid-power
sudo chmod 755 ${DEST}/cloonix-suid-power
sudo chmod u+s ${DEST}/cloonix-suid-power
cd ${HERE}/../lib_io_clownix
make clean
cd ${HERE}
make clean

