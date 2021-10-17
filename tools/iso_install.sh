#!/bin/bash
HERE=`pwd`
NAME=bullseye
ISO=/home/perrier/Bureau/archives/iso/debian-10.2.0-amd64-netinst.iso
ISO=/home/perrier/Bureau/archives/iso/debian-9.11.0-amd64-netinst.iso
BULK=${HOME}/cloonix_data/bulk
QCOW2=${BULK}/${NAME}_iso.qcow2
CDROM="-cdrom ${ISO}"
CLOONIX_BIN="/usr/local/bin/cloonix"
QEMU_BIN="${CLOONIX_BIN}/server/qemu"
#----------------------------------------------------------------------------
if [ ! -e ${ISO} ]; then
  echo missing ${ISO}
  exit 1
fi
if [ -e ${QCOW2} ]; then
  echo
  echo GOING TO ERASE ${QCOW2} in 2 sec
  echo
  sleep 3
  rm -f ${QCOW2}
fi
qemu-img create -f qcow2 ${QCOW2} 30G
${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 3000 \
                                  -cpu host,+vmx -smp 4 -no-reboot \
                                  -drive file=${QCOW2},media=disk,if=virtio \
                                  -boot d ${CDROM}
