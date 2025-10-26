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
make clean
make

sudo mv ${HERE}/cloonix-main-server ${DEST}
sudo chown root:root ${DEST}/cloonix-main-server
sudo chmod 755 ${DEST}/cloonix-main-server

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

