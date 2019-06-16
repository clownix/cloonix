#!/bin/bash
HERE=`pwd`
source ./confvars
if [ ! -e ${QCOW2} ]; then
  echo missing ${QCOW2}
  exit 1
fi

if [ "${TYPE}" = "csr1000v" ]; then
  sudo ip tuntap add dev tap77 mode tap
  # sudo ip tuntap del dev tap77 mode tap
  ${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 8000 \
                   -cpu host,+vmx -smp 4 -no-reboot \
                   -drive file=${QCOW2},index=0,media=disk,if=virtio \
                   -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
                   -netdev type=tap,id=net0,ifname=tap77 \
                   -device virtio-net-pci,netdev=net0,mac=52:54:00:81:f5:44
else
  ${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 8000 \
                   -cpu host,+vmx -smp 4 -no-reboot \
                   -drive file=${QCOW2},index=0,media=disk,if=virtio
fi

