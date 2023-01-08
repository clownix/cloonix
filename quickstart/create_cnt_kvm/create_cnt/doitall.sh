#!/bin/bash
set -e

HERE=`pwd`

LIST="centos9 \
      openwrt \
      jammy \
      bullseye \
      fedora36 \
      bookworm \
      opensuse155"


CLOONIX_BULK=${HOME}/cloonix_data/bulk
mkdir -p ${CLOONIX_BULK}
sudo echo "sudoer rights given"

for i in ${LIST}; do
  if [ -e ${CLOONIX_BULK}/${i}.img ]; then
    echo ${CLOONIX_BULK}/${i}.img
    echo must be erased first
    exit 1
  fi
  sudo rm -f /root/${i}
  sudo rm -f /root/${i}.img
done

for i in ${LIST}; do
  echo BEGIN ${i} 
  sudo ${HERE}/${i}
  sudo mv -v /root/${i}.img ${CLOONIX_BULK}
  echo END ${i}  
done

if [ -e /usr/bin/podman ]; then
  sudo ./podman_bookworm.sh
fi
if [ -e /usr/bin/docker ]; then
  sudo ./docker_bookworm.sh
fi
