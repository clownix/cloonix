#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system-x86_64"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/server/qemu"
QCOW2="/var/lib/cloonix/bulk/installed_arch.qcow2"
ISO=/home/perrier/Bureau/archives/archlinux-2023.07.01-x86_64.iso

if [ ! -e ${ISO} ]; then
  echo missing ${ISO}
  exit 1
fi
if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi

rm -f ${QCOW2}
qemu-img create -f qcow2 ${QCOW2} 60G


${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 \
     -vga virtio -display gtk \
     -nodefaults \
     -drive file=${QCOW2},index=0,media=disk,if=virtio \
     -boot d \
     -cdrom ${ISO}
echo
echo DONE ${QCOW2}



