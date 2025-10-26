#!/bin/bash
HERE=`pwd`
DEST="/usr/libexec/cloonix/cloonfs/bin"

for i in "lib_io_clownix" \
         "lib_rpc_clownix" \
         "lib_rpc_doors" \
         "lib_rpc_layout" \
         "lib_utils"; do 
  cd ${HERE}/../${i}
  make clean
  make
done

cd ${HERE}
./cmd

sudo mv ${HERE}/cloonix-doorways  ${DEST}
sudo chown root:root ${DEST}/cloonix-doorways
sudo chmod 755 ${DEST}/cloonix-doorways

for i in "lib_io_clownix" \
         "lib_rpc_clownix" \
         "lib_rpc_doors" \
         "lib_rpc_layout" \
         "lib_utils"; do
  cd ${HERE}/../${i}
  make clean
done

cd ${HERE}
make clean

