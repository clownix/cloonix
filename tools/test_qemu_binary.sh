#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"
QCOW2=${HOME}/cloonix_data/bulk/tester.qcow2

${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 \
            -vga virtio -display gtk,gl=on \
            -drive file=${QCOW2},index=0,media=disk,if=virtio 
echo
echo DONE ${QCOW2}




