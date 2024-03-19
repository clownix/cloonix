#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system-x86_64"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/server/qemu"
CISCO_ISO=/media/perrier/Samsung_T5/archives/iso/c8000v-universalk9.17.13.01a.iso

CISCO_QCOW2=/var/lib/cloonix/bulk/c8000.qcow2

if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  exit 1
fi
if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi

rm -f ${CISCO_QCOW2}
qemu-img create -f qcow2 ${CISCO_QCOW2} 60G

sync
sleep 1

${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -vga virtio \
     -nodefaults \
     -drive file=${CISCO_QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
     -boot d \
     -cdrom ${CISCO_ISO}

echo
echo
echo DONE ${CISCO_QCOW2}



