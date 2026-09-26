#!/bin/bash
# Build the cisco Catalyst 8000V image, WITHOUT sudo.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
. ${HERE}/cisco_common.sh
usage() {
  cat <<EOF
doitall.sh -- build the cisco image for cloonix, without sudo.

Usage: ./doitall.sh <what> [iso]

  --iso              the day-0 disc only
  --install <iso>    the install only, from the Cisco iso
  --dayzero          apply the day-0 disc to an installed qcow2
  --all <iso>        the three in order
  -h, --help         this text

The Cisco iso is given on the command line or in CISCO_ISO:

  ./doitall.sh --all /path/to/c8000v-universalk9.17.18.01a.iso

Result   : ${QCOW2}
Console  : vnc ${VNC} (tcp 59${VNC##*:}) while building, serial once built

Once built, the demo:  cloonix cli <net> demo cisco.py
EOF
}
if [ "$#" = "0" ]; then
  usage
  exit 0
fi
if [ "$(id -u)" = "0" ]; then
  echo "FATAL: do not run this as root."
  echo "       Nothing here needs it, and a root-owned ${QCOW2} would"
  echo "       block later runs."
  exit 1
fi
DO_ISO=0
DO_INSTALL=0
DO_DAYZERO=0
ISO="${CISCO_ISO:-}"
for arg in "$@"; do
  case "${arg}" in
    --iso)      DO_ISO=1 ;;
    --install)  DO_INSTALL=1 ;;
    --dayzero)  DO_DAYZERO=1 ;;
    --all)      DO_ISO=1; DO_INSTALL=1; DO_DAYZERO=1 ;;
    -h|--help)  usage; exit 0 ;;
    -*) echo "unknown option: ${arg}"; echo "try: $0 --help"; exit 1 ;;
    *)  ISO="${arg}" ;;
  esac
done
if [ "${DO_INSTALL}" = "1" ] && [ -z "${ISO}" ]; then
  echo "FATAL: no Cisco iso given."
  echo "try: $0 --help"
  exit 1
fi
run() {
  echo
  echo "----------------------------------------------------------------------"
  echo " $* ($(date '+%H:%M:%S'))"
  echo "----------------------------------------------------------------------"
  "$@" || { echo; echo "FAILED: $*"; exit 1; }
}
START=$(date +%s)
[ "${DO_ISO}" = "1" ]     && run ${HERE}/step1_dayzero_iso.sh
[ "${DO_INSTALL}" = "1" ] && run ${HERE}/step2_make_qcow2.sh "${ISO}"
[ "${DO_DAYZERO}" = "1" ] && run ${HERE}/step3_apply_dayzero.sh
END=$(date +%s)
echo
echo "======================================================================"
echo " ALL OK  (total $(( (END-START)/60 ))m $(( (END-START)%60 ))s)"
[ -f ${QCOW2} ] && printf "  kvm %-12s %s\n" "${NAME}" \
                          "$(du -sh ${QCOW2} 2>/dev/null | cut -f1)"
echo "======================================================================"
