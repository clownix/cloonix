#!/bin/bash

if [ ! -d ${HOME}/cloonix_data/bulk ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${HOME}/cloonix_data/bulk
  exit 1
fi


rm -f ${HOME}/cloonix_data/bulk/chr-6.49.img.zip
rm -f ${HOME}/cloonix_data/bulk/chr-6.49.img
rm -f ${HOME}/cloonix_data/bulk/mikrotik.qcow2

cd ${HOME}/cloonix_data/bulk

wget https://download.mikrotik.com/routeros/6.49/chr-6.49.img.zip

unzip chr-6.49.img.zip 

qemu-img convert -O qcow2 chr-6.49.img ${HOME}/cloonix_data/bulk/mikrotik.qcow2






