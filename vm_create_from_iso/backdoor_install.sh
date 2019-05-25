#!/bin/bash
HERE=`pwd`
source ./confvars
if [ ! -e ${QCOW2} ]; then
  echo missing ${QCOW2}
  exit 1
fi
${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 6000 \
                 -cpu host,+vmx -smp 4 -no-reboot \
                 -chardev socket,id=qmpsock,path=/tmp/qemu_qmp_sock,server \
                 -mon chardev=qmpsock,mode=control \
                 -drive file=${QCOW2},index=0,media=disk,if=virtio &

sleep 2
./qmp_cnf ${TYPE} ${LANG}
