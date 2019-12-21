#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
CISCO_QCOW2=${HOME}/cloonix_data/bulk/ecisco.qcow2
if [ ! -e ${CISCO_QCOW2} ]; then
  echo missing ${CISCO_QCOW2}
  exit 1
fi
sudo ip tuntap add dev tap91 mode tap
sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
            -netdev type=tap,id=net0,ifname=tap91 \
            -device virtio-net-pci,netdev=net0,mac=52:ca:fe:de:ca:40

sleep 10
sudo ip tuntap del dev tap91 mode tap

echo DONE cisco.qcow2




