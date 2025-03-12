#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/server/qemu"
ISO="/media/perrier/T7Shield/from_iso/Fedora-Workstation-Live-x86_64-41-1.4.iso"
QCOW2="/var/lib/cloonix/bulk/fedora_iso.qcow2"
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
     -serial none \
     -vga virtio \
     -nodefaults \
     -drive file=${QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -chardev socket,path=/tmp/totototo,server=on,wait=off,id=cloon0 \
     -device virtio-serial-pci \
     -device virtserialport,chardev=cloon0,name=net.cloon.0 \
     -chardev socket,path=/tmp/titititi,server=on,wait=off,id=qga0 \
     -device virtio-serial-pci \
     -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0 \
     -chardev spicevmc,id=spice0,name=vdagent \
     -device virtio-serial-pci \
     -device virtserialport,chardev=spice0,name=com.redhat.spice.0 \
     -boot d \
     -cdrom ${ISO}
#------------------------------------------------------------------------
echo
echo
echo DONE ${QCOW2}



