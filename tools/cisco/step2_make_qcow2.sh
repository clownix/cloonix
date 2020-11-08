#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"
CISCO_ISO=/media/perrier/Samsung_T51/iso/csr1000v-universalk9.17.02.01r.iso

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
     -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio,cache=none \
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
sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
     -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -serial stdio \
     -nographic \
     -nodefaults \
     -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio,cache=none \
     -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
     -netdev type=tap,id=net71,vhost=on,ifname=tap71,script=no,downscript=no \
     -device virtio-net-pci,netdev=net71 \
     -netdev type=tap,id=net72,vhost=on,ifname=tap72,script=no,downscript=no \
     -device virtio-net-pci,netdev=net72 \
     -netdev type=tap,id=net73,vhost=on,ifname=tap73,script=no,downscript=no \
     -device virtio-net-pci,netdev=net73 \
     -netdev type=tap,id=net74,vhost=on,ifname=tap74,script=no,downscript=no \
     -device virtio-net-pci,netdev=net74 \
     -netdev type=tap,id=net75,vhost=on,ifname=tap75,script=no,downscript=no \
     -device virtio-net-pci,netdev=net75 \
     -cdrom ${CISCO_PRECONFIG_ISO}
echo
echo


