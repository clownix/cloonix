#!/bin/bash
set -e

sudo ./openwrt.sh

LIST="fedora41 \
      noble \
      bookworm"

sudo echo "sudoer rights given"
mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.qcow2 ]; then
    echo /var/lib/cloonix/bulk/${i}.qcow2
    echo already exists
  else
    echo BEGIN ${i} 
    sudo ./${i}.sh
    echo END ${i}  
  fi
done
