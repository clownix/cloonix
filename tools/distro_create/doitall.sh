#!/bin/bash
set -e

HERE=`pwd`

LIST="centos8 \
      fedora31 \
      opensuse152 \
      eoan \
      buster \
      bullseye \
      openwrt"


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

