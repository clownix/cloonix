#!/bin/bash
# Build a config disc: one iso holding iosxe_config.txt at its root, which is
# the name IOS-XE looks for on a mounted cdrom at boot.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
. ${HERE}/cisco_common.sh

SRC="${1:-${HERE}/configs/iosxe_config.txt}"
OUT="${2:-${DAYZERO_ISO}}"
case "${OUT}" in /*) ;; *) OUT="${BULK}/${OUT}" ;; esac
[ -e "${SRC}" ] || { echo "missing ${SRC}"; exit 1; }

XORRISOFS="${XORRISO} -as mkisofs"
[ -x ${XORRISO} ] || XORRISOFS="/usr/bin/xorrisofs"
[ -x ${XORRISOFS%% *} ] || { echo "no xorriso found"; exit 1; }

rm -rf ${WORK}
mkdir -p ${WORK}/cdrom
cp "${SRC}" ${WORK}/cdrom/iosxe_config.txt
${XORRISOFS} -o "${OUT}" ${WORK}/cdrom

echo
echo DONE "${OUT}"
