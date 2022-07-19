#!/bin/bash

HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"
BULK="${HOME}/cloonix_data/bulk"
NAME="mikrotik"
ISO_NAME="mikrotik-7.4rc2.iso"
ISO_HTTP="https://download.mikrotik.com/routeros/7.4rc2/"
ISO="${BULK}/${ISO_NAME}"
QCOW2="${BULK}/${NAME}.qcow2"

if [ ! -e ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 ]; then
  echo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 does not exist
  echo Install cloonix
  exit 1
fi

if [ ! -d ${BULK} ]; then
  echo directory ${BULK} does not exist:
  echo mkdir -p ${BULK}
  exit 1
fi

if [ ! -e ${ISO} ]; then
  wget ${ISO_HTTP}/${ISO_NAME}
  if [ ! -e ${ISO} ]; then
    echo ERROR could not get ${ISO_NAME}
    exit 1
  fi
fi

rm -f ${BULK}/${NAME}.qcow2
qemu-img create -f qcow2 ${QCOW2} 5G
sync
sleep 1
sync

echo Press i to install with following script, when mikrotik.qcow2 is done, then launch it
echo with cloonix adding the options: --no_qemu_ga and --natplug_flag.
echo Then admin to get in the spice console, return for password then request dhclient as
echo follows: ip/dhcp-client/add interface=ether1. After that a double-click on the vm
echo should get a terminal.

${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
     -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 2000 \
     -cpu host,+vmx \
     -smp 4 \
     -vga virtio \
     -nodefaults \
     -no-reboot \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -drive if=none,id=hd,file=${QCOW2} \
     -boot d \
     -cdrom ${ISO}

echo
echo
echo DONE ${QCOW2}

exit
    

${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
     -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 2000 \
     -cpu host,+vmx \
     -smp 4 \
     -vga virtio \
     -no-reboot \
     -nodefaults \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -drive if=none,id=hd,file=${QCOW2}





