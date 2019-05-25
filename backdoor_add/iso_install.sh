#!/bin/bash
HERE=`pwd`
source ./confvars
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
                               -drive file=${QCOW2},index=0,media=disk,if=virtio \
                               -boot d ${CDROM}
