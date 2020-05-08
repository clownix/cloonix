#!/bin/bash
if [ $# != 1 ]; then
  echo give a qcow2 file
  echo
  exit -1
fi
QCOW2=${1}
if [ ! -e $QCOW2 ]; then
  echo NOT FOUND!
  echo $QCOW2
  echo
  exit -1
fi

kvm -enable-kvm -m 6000 -cpu host,+vmx -smp 4 \
    -drive file=${QCOW2},index=0,media=disk,if=virtio \
    -netdev type=tap,id=net71,ifname=tap71,script=no,downscript=no \
    -device virtio-net-pci,netdev=net71


