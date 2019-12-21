#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
CISCO_QCOW2=${HOME}/cloonix_data/bulk/ecisco.qcow2
CISCO_ISO=${HOME}/Bureau/archives/cisco/csr1000v-universalk9.16.10.01b.iso
if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  exit 1
fi

qemu-img create -f qcow2 ${CISCO_QCOW2} 60G

sudo ip tuntap add dev tap77 mode tap
sleep 2

echo
echo IMPORTANT: At grub choice, choose the serial start second line!
echo
echo waiting 5
sleep 5

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
            -netdev type=tap,id=net0,ifname=tap77 \
            -device virtio-net-pci,netdev=net0,mac=52:ca:fe:de:ca:40 \
            -boot d -cdrom ${CISCO_ISO}
if [ ! -e ${CISCO_QCOW2} ]; then
  echo missing ${CISCO_QCOW2}
  sudo ip tuntap del dev tap77 mode tap
  exit 1
fi
echo
echo After the cisco start, at question:
echo Would you like to enter the initial configuration dialog? [yes/no]: 
echo answer no, terminate auto-install,
echo at prompt, type \"enable\" and \"conf t\",
echo cut paste config in file nvram_csr_cnf.txt
echo
echo waiting 10
sleep 10

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
            -netdev type=tap,id=net0,ifname=tap77 \
            -device virtio-net-pci,netdev=net0,mac=52:ca:fe:de:ca:40

sleep 5
sudo ip tuntap del dev tap77 mode tap
if [ ! -e ${CISCO_QCOW2} ]; then
  echo missing ${CISCO_QCOW2}
  exit 1
fi
echo DONE ${CISCO_QCOW2}




