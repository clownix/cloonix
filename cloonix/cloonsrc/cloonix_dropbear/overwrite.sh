#!/bin/bash
HERE=`pwd`
DEST="/usr/libexec/cloonix/cloonfs/bin"

./cmd

for i in "cloonix-dropbear-ssh" "cloonix-dropbear-scp"; do
  sudo mv ${HERE}/${i} ${DEST}
  sudo chown root:root ${DEST}/${i}
  sudo chmod 755 ${DEST}/${i}
done

./allclean


