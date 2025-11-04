#!/bin/bash
HERE=`pwd`
DEST="/usr/libexec/cloonix/cloonfs/bin"
./cmd
sudo mv ${HERE}/crun_tainted/crun ${DEST}/cloonix-crun
sudo chown root:root ${DEST}/cloonix-crun
sudo chmod 755 ${DEST}/cloonix-crun
cd ${HERE}/../../../quickstart/cloonix_toy
./create_toy_targz.sh
cd ${HERE}
./allclean


