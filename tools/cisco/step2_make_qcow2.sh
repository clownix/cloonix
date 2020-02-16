#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
CISCO_QCOW2=${HOME}/cloonix_data/bulk/cisco.qcow2
CISCO_ISO=/media/perrier/Samsung_T5/iso/csr1000v-universalk9.16.09.01.iso
CISCO_PRECONFIG_ISO=${HERE}/pre_configs/preconfig_cisco.iso
if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  exit 1
fi
if [ ! -e ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 ]; then
  echo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 does not exist
  echo Install cloonix
  exit 1
fi
if [ ! -d ${HOME}/cloonix_data/bulk ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${HOME}/cloonix_data/bulk
  exit 1
fi

rm ${CISCO_QCOW2}
qemu-img create -f qcow2 ${CISCO_QCOW2} 60G

echo
echo
echo IMPORTANT: Choose the \"Serial Console\"
echo
echo            Second line in the grub menu!
echo
sleep 2

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
            -boot d \
            -cdrom ${CISCO_ISO}

echo
echo
echo DONE ${CISCO_QCOW2}

echo
echo
echo Loading the preconfiguration...
sleep 2
echo
echo
sudo ip tuntap add dev tap71 mode tap
sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
            -netdev type=tap,id=net0,ifname=tap71 \
            -device virtio-net-pci,netdev=net0,mac=52:ca:fe:de:ca:40 \
            -cdrom ${CISCO_PRECONFIG_ISO}
echo
sudo ip tuntap del dev tap71 mode tap
echo




