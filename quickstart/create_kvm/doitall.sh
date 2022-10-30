#!/bin/bash
set -e

HERE=`pwd`

LIST="openwrt \
      jammy \
      centos8 \
      bullseye \
      fedora36 \
      bookworm"


CLOONIX_BULK=${HOME}/cloonix_data/bulk
mkdir -p ${CLOONIX_BULK}
sudo echo "sudoer rights given"

for i in ${LIST}; do
  if [ -e ${CLOONIX_BULK}/${i}.qcow2 ]; then
    echo ${CLOONIX_BULK}/${i}.qcow2
    echo must be erased first
    exit 1
  fi
  sudo rm -f /root/${i}
  sudo rm -f /root/${i}.qcow2
done

for i in ${LIST}; do
  echo BEGIN ${i} 
  sudo ${HERE}/${i}
  sudo mv -v /root/${i}.qcow2 ${CLOONIX_BULK}
  echo END ${i}  
done

sudo ${HERE}/opensuse155
sudo mv -v /root/opensuse155.qcow2 ${CLOONIX_BULK}
./tumbleweed
