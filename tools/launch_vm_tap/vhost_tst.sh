#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"

if [ ! -e ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 ]; then
  echo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 does not exist
  echo Install cloonix
  exit 1
fi
if [ ! -d ${HOME}/cloonix_data/bulk ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${HOME}/cloonix_data/bulk
  exit 1
fi


VM=tst.qcow2

QCOW2=${HOME}/cloonix_data/bulk/${VM}


if [ ! -f $QCOW2 ]; then
  echo $QCOW2
  echo does not exist
  exit
fi

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
  -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
  -cpu host,+vmx -smp 4 \
  -serial stdio \
  -drive file=${QCOW2},index=0,media=disk,if=virtio \
  -netdev type=tap,id=netvhost0,vhost=on,ifname=vhost71,script=no,downscript=no \
  -device virtio-net-pci,netdev=netvhost0

