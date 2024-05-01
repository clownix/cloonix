#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system"
CLOONIX_QEMU_BIN_DIR="/usr/libexec/cloonix/server/qemu"
TARGET=/tmp/cisco_initial_configuration
CISCO_PRECONFIG_ISO=${TARGET}/iosxe_config.iso
CISCO_QCOW2=/var/lib/cloonix/bulk/c8000.qcow2

if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi
if [ ! -e ${CISCO_PRECONFIG_ISO} ]; then
  echo missing ${CISCO_PRECONFIG_ISO}
  exit 1
fi

echo
echo
echo Loading the preconfiguration...
sleep 1
echo
echo
sudo ${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_BIN_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -vga virtio \
     -nodefaults \
     -drive file=${CISCO_QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
     -netdev type=tap,id=net71,vhost=on,ifname=tap71,script=no,downscript=no \
     -device virtio-net-pci,netdev=net71 \
     -cdrom ${CISCO_PRECONFIG_ISO}
echo
echo


