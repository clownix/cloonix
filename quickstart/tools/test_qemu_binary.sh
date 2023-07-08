#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system-x86_64"
CLOONIX_QEMU_BIN_DIR="/usr/libexec/cloonix/server/qemu"
QCOW2="/var/lib/cloonix/bulk/tester.qcow2"

${CLOONIX_QEMU_BIN} \
            -L ${CLOONIX_QEMU_BIN_DIR} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 \
            -vga virtio -display gtk \
            -drive file=${QCOW2},index=0,media=disk,if=virtio \
            -nodefaults
echo
echo DONE ${QCOW2}




