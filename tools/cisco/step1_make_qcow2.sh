#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
CISCO_QCOW2=${HOME}/cloonix_data/bulk/cisco.qcow2
CISCO_ISO=/media/perrier/Samsung_T5/iso/csr1000v-universalk9.16.09.01.iso
if [ ! -e ${CISCO_ISO} ]; then
  echo missing ${CISCO_ISO}
  echo Found mine here \(may have disapeared\):
  echo wget http://185.136.162.5/CSR1000v/csr1000v-universalk9.16.09.01.iso
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

qemu-img create -f qcow2 ${CISCO_QCOW2} 60G

echo
echo
echo IMPORTANT: At grub choice, choose the \"Serial Console\"
echo            second line in the grub menu!
echo
echo
echo Then wait until end.
echo
echo
echo waiting 5 sec before start...
sleep 5

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 -no-reboot \
            -serial stdio \
            -nographic \
            -nodefaults \
            -drive file=${CISCO_QCOW2},index=0,media=disk,if=virtio \
            -uuid 3824cca6-7603-423b-8e5c-84d15d9b0a6a \
            -boot d -cdrom ${CISCO_ISO}

echo
echo
echo DONE ${CISCO_QCOW2}
echo
echo




