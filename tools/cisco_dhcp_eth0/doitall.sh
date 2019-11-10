#!/bin/bash
HERE=`pwd`
. ${HERE}/config.env

rm -f ${CLOONIX_BULK}/cisco.qcow2
rm -rf ${CISCO_OUTPUTS}
mkdir -p ${CISCO_OUTPUTS}

if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  exit 1
fi

if [ -e ${CISCO_QCOW2} ]; then
  echo
  echo GOING TO ERASE ${CISCO_QCOW2} in 2 sec
  echo
  sleep 3
  rm -f ${CISCO_QCOW2}
fi

qemu-img create -f qcow2 ${CISCO_QCOW2} 60G

sudo ip tuntap add dev tap77 mode tap

sleep 2

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
            -netdev type=tap,id=net0,ifname=tap77 \
            -device virtio-net-pci,netdev=net0,mac=52:54:00:81:f5:40 \
            -boot d -cdrom ${CISCO_ISO}
sleep 10
sudo ip tuntap del dev tap77 mode tap

echo waiting 30
sleep 30

if [ ! -e ${CISCO_QCOW2} ]; then
  echo missing ${CISCO_QCOW2}
  exit 1
fi

sudo ip tuntap add dev tap77 mode tap
sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
             -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
             -cpu host,+vmx -smp 4 -no-reboot \
             -chardev socket,id=qmpsock,path=/tmp/qemu_qmp_sock,server \
             -mon chardev=qmpsock,mode=control \
             -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
             -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
             -netdev type=tap,id=net0,ifname=tap77 \
             -device virtio-net-pci,netdev=net0,mac=52:54:00:81:f5:40 &
sleep 2
sudo ./qmp_diag
sleep 10
sudo ip tuntap del dev tap77 mode tap
if [ ! -e ${CISCO_QCOW2} ]; then
  echo missing ${CISCO_QCOW2}
  exit 1
fi
cp -f ${CISCO_QCOW2} ${CLOONIX_BULK}/cisco.qcow2
echo DONE cisco.qcow2
