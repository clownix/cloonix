#!/bin/bash
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
. ${HERE}/cisco_common.sh

[ -e ${QCOW2} ] || { echo "missing ${QCOW2}, run step2 first"; exit 1; }
[ -e ${DAYZERO_ISO} ] || { echo "missing ${DAYZERO_ISO}, run step1 first"; exit 1; }
cisco_check_tools

echo
echo "applying ${DAYZERO_ISO}"
echo "vnc console on ${VNC}, several minutes"
echo
cisco_qemu "${DAYZERO_ISO}" c

echo
echo DONE ${QCOW2}
echo "cloonix cli <net> add cisco cisco1 ram=${RAM} cpu=${CPU} eth=3 ${NAME}.qcow2"
