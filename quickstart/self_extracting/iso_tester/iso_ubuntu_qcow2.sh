#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/server/qemu"
ISO="/media/perrier/T7Shield/from_iso/ubuntu-24.04.2-desktop-amd64.iso"
QCOW2="/var/lib/cloonix/bulk/ubuntu_iso.qcow2"
#------------------------------------------------------------------------
if [ ! -e ${ISO} ]; then
  echo missing ${ISO}
  exit 1
fi
if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi
#------------------------------------------------------------------------
rm -f ${QCOW2}
qemu-img create -f qcow2 ${QCOW2} 60G
#------------------------------------------------------------------------
sync
sleep 1

#------------------------------------------------------------------------
${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -vga virtio \
     -nodefaults \
     -drive file=${QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -boot d \
     -cdrom ${ISO}
#------------------------------------------------------------------------
echo
echo
echo DONE ${QCOW2}



