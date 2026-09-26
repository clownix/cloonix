#!/bin/bash
# Install a Cisco Catalyst 8000V from its iso into ${QCOW2}.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
. ${HERE}/cisco_common.sh

CISCO_ISO="${1:-${CISCO_ISO:-}}"
if [ -z "${CISCO_ISO}" ] || [ ! -e "${CISCO_ISO}" ]; then
  echo "usage: $0 <c8000v-universalk9.<version>.iso>"
  echo "       (or export CISCO_ISO=<path>)"
  exit 1
fi
cisco_check_tools
command -v qemu-img >/dev/null 2>&1 || { echo "qemu-img required"; exit 1; }
[ -d ${BULK} ] || { echo "${BULK} does not exist"; exit 1; }
[ -w ${BULK} ] || { echo "${BULK} not writable by $(id -un)"; exit 1; }

rm -f ${QCOW2}
qemu-img create -f qcow2 ${QCOW2} ${DISK_SIZE_G}G
sync

echo
echo "installing ${CISCO_ISO}"
echo "vnc console on ${VNC}"
echo
cisco_qemu "${CISCO_ISO}" d

echo
echo DONE ${QCOW2}
echo "next: ./step3_apply_dayzero.sh"
