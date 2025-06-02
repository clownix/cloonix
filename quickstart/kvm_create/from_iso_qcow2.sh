#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/cloonfs/cloonix-qemu-system"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/cloonfs/qemu"
CISCO_ISO="/media/perrier/Samsung_T5/from_iso/debian-12.11.0-amd64-DVD-1.iso"
CISCO_QCOW2="/var/lib/cloonix/bulk/from_iso.qcow2"
#------------------------------------------------------------------------
if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  exit 1
fi
if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi
#------------------------------------------------------------------------
rm -f ${CISCO_QCOW2}
qemu-img create -f qcow2 ${CISCO_QCOW2} 60G
#------------------------------------------------------------------------
sync
sleep 1

#------------------------------------------------------------------------
${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -vga virtio \
     -nodefaults \
     -drive file=${CISCO_QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -boot d \
     -cdrom ${CISCO_ISO}
#------------------------------------------------------------------------
echo
echo
echo DONE ${CISCO_QCOW2}



