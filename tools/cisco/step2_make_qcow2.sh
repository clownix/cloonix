#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
CISCO_ISO=/media/perrier/Samsung_T5/iso/csr1000v-universalk9.16.09.01.iso

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

TYPE=cisco4

CISCO_PRECONFIG_ISO=${HERE}/pre_configs/preconfig_${TYPE}.iso
if [ ! -e ${CISCO_PRECONFIG_ISO} ]; then
  echo missing ${CISCO_PRECONFIG_ISO}
  exit 1
fi
CISCO_QCOW2=${HOME}/cloonix_data/bulk/${TYPE}.qcow2

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
for i in tap71 tap72 tap73 tap74 tap75 ; do
  sudo ip tuntap add dev ${i} mode tap
done

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
            -netdev type=tap,id=net71,ifname=tap71 \
            -device virtio-net-pci,netdev=net71 \
            -netdev type=tap,id=net72,ifname=tap72 \
            -device virtio-net-pci,netdev=net72 \
            -netdev type=tap,id=net73,ifname=tap73 \
            -device virtio-net-pci,netdev=net73 \
            -netdev type=tap,id=net74,ifname=tap74 \
            -device virtio-net-pci,netdev=net74 \
            -netdev type=tap,id=net75,ifname=tap75 \
            -device virtio-net-pci,netdev=net75 \
            -cdrom ${CISCO_PRECONFIG_ISO}
echo
for i in tap71 tap72 tap73 tap74 tap75 ; do
  sudo ip tuntap del dev ${i} mode tap
done
echo


