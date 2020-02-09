#!/bin/bash
if [ $# != 2 ]; then
  echo "$0 <vm_to_adapt.qcow2> <fr or us>"
  exit 1
fi
QCOW2_NAME=$1
BULK=${HOME}/cloonix_data/bulk/
QCOW2=${BULK}/${QCOW2_NAME}
LANG=$2
case "${LANG}" in
  "fr" | "us")
    echo LANG=$LANG
  ;;

  *)
    echo BAD LANG=${LANG}, Accepted values: fr or us
    echo "$0 <vm_to_adapt.qcow2> <fr or us>"
    exit 1
  ;;
esac

if [ ! -e ${QCOW2} ]; then
  echo missing ${QCOW2}
  echo "$0 <vm_to_adapt.qcow2> <fr or us>"
  echo vm_to_adapt.qcow2 must be in $BULK
  exit 1
fi
QEMU_BIN="/usr/local/bin/cloonix/server/qemu/qemu_bin"
${QEMU_BIN}/qemu-system-x86_64 -L ${QEMU_BIN} -enable-kvm -m 3000 \
                 -cpu host,+vmx -smp 4 -no-reboot \
                 -chardev socket,id=qmpsock,path=/tmp/qemu_qmp_sock,server \
                 -mon chardev=qmpsock,mode=control \
                 -drive file=${QCOW2},index=0,media=disk,if=virtio &
sleep 2
./qmp_backdoor.bin ${LANG}
