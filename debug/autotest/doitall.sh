#!/bin/bash
# Builds every kvm_autotest_<distro>.qcow2 from <distro>.qcow2 of bulk;
# needs cloonix and the mirror (each boots on nemo, nat to 10.117.0.1).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
BULK="/var/lib/cloonix/bulk"
LOGDIR="/tmp/autotest-doitall-logs-$(id -u)"
DISTROS="trixie alpine arch fedora44 resolute rocky tumbleweed"
usage() {
  cat <<EOF
doitall.sh -- build the autotest machines, kvm_autotest_<distro>.qcow2.

Usage: ./doitall.sh --all | <distro> ...

  --all              every machine, in this order: ${DISTROS}
  <distro> ...       only those
  --list             print the list, build nothing
  -h, --help         this text

With no argument this help is printed and nothing is built. Each machine
is made from ${BULK}/<distro>.qcow2, so the base must exist first:
  cd ../../vm_create/cnt_kvm && ./doitall.sh --kvm <distro>
A build stops at the first failure. Logs: ${LOGDIR}/<distro>.log
EOF
}
if [ "$#" = "0" ]; then
  usage
  exit 0
fi
SELECTED=""
for arg in "$@"; do
  case "${arg}" in
    --all|-all|all)   SELECTED="${DISTROS}" ;;
    --list|-list|list) echo "${DISTROS}"; exit 0 ;;
    -h|--help|-help|help) usage; exit 0 ;;
    -*) echo "unknown option: ${arg}"; echo "try: $0 --help"; exit 1 ;;
    *)  SELECTED="${SELECTED} ${arg}" ;;
  esac
done
for d in ${SELECTED}; do
  case " ${DISTROS} " in
    *" ${d} "*) : ;;
    *) echo "FATAL: unknown distro '${d}', one of: ${DISTROS}"; exit 1 ;;
  esac
  [ -x "${HERE}/kvm_autotest_${d}.sh" ] || \
    { echo "FATAL: ${HERE}/kvm_autotest_${d}.sh is missing"; exit 1; }
  [ -f "${BULK}/${d}.qcow2" ] || \
    { echo "FATAL: ${BULK}/${d}.qcow2 is missing: cd ../../vm_create/cnt_kvm && ./doitall.sh --kvm ${d}"; exit 1; }
done
command -v cloonix >/dev/null 2>&1 || { echo "FATAL: cloonix is not installed"; exit 1; }
mkdir -p "${LOGDIR}"
START=$(date +%s)
for d in ${SELECTED}; do
  log="${LOGDIR}/${d}.log"
  echo
  echo "----------------------------------------------------------------------"
  echo " kvm_autotest_${d}   ($(date '+%H:%M:%S'))"
  echo "----------------------------------------------------------------------"
  t0=$(date +%s)
  rcfile="${LOGDIR}/.rc_${d}"
  rm -f "${rcfile}"
  ( cd "${HERE}" && "./kvm_autotest_${d}.sh"; echo $? > "${rcfile}" ) 2>&1 | tee "${log}"
  rc=$(cat "${rcfile}" 2>/dev/null || echo 127)
  rm -f "${rcfile}"
  if [ "${rc}" != "0" ]; then
    echo
    echo "======================================================================"
    echo " FAILED: kvm_autotest_${d}.sh (exit ${rc}) after $(( $(date +%s) - t0 ))s"
    echo " log:    ${log}"
    echo " Nothing after this point has been built. Once fixed: ./doitall.sh ${d}"
    echo "======================================================================"
    exit "${rc}"
  fi
  echo " ok: kvm_autotest_${d}.qcow2 in $(( $(date +%s) - t0 ))s"
done
END=$(date +%s)
echo
echo "======================================================================"
echo " ALL OK  (total $(( (END-START)/60 ))m $(( (END-START)%60 ))s)"
echo "======================================================================"
for d in ${SELECTED}; do
  printf "  %-32s %s\n" "kvm_autotest_${d}.qcow2" \
         "$(du -sh "${BULK}/kvm_autotest_${d}.qcow2" 2>/dev/null | cut -f1)"
done
echo "======================================================================"
