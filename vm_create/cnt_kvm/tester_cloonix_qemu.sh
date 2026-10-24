#!/bin/bash
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/cloonfs/bin/cloonix-qemu-system"
CLOONIX_QEMU_DIR="/usr/libexec/cloonix/cloonfs/bin/qemu"
QCOW2="/var/lib/cloonix/bulk/tumbleweed.qcow2"
if [ ! -e ${CLOONIX_QEMU_BIN} ]; then
  echo ${CLOONIX_QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi
# no-reboot: a boot failure stays on screen instead of looping.
${CLOONIX_QEMU_BIN} \
     -L ${CLOONIX_QEMU_DIR} -enable-kvm -m 6000 \
     -machine q35 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -nodefaults -nographic \
     -chardev stdio,id=char0,signal=off \
     -serial chardev:char0 \
     -drive file=${QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -device virtio-serial-pci,id=vser0 \
     -chardev socket,path=/tmp/qga-test.sock,server=on,wait=off,id=qga0 \
     -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0
echo
echo
echo DONE
