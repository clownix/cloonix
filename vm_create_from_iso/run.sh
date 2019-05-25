#!/bin/bash
HERE=`pwd`
source ./confvars
if [ ! -e ${QCOW2} ]; then
  echo missing ${QCOW2}
  exit 1
fi
${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 8000 \
                 -cpu host,+vmx -smp 4 -no-reboot \
                 -drive file=${QCOW2},index=0,media=disk,if=virtio

