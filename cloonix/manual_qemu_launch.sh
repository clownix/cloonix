#!/bin/bash
HERE=`pwd`
QCOW2=${HOME}/cloonix_data/bulk/stretch.qcow2
DISK="-drive file=${QCOW2},index=0,media=disk,if=virtio"
QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
export LD_LIBRARY_PATH="/usr/local/bin/cloonix/common/spice/spice_lib"
QEMU="${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} \
     -enable-kvm -m 2000 -cpu host,+vmx -smp 4 -no-reboot" 

${QEMU} ${DISK}

