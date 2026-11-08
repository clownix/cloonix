#!/bin/bash
# The complete renewal: every image of cnt_kvm and of cnt_kvm_demo erased,
# in bulk and in four directories of the home, built again by the two
# doitall.sh, then delivered, the containers as they are, the qcow2 gzipped.
# Detached: launched, it goes on alone, once it has checked that every
# image can be built again. A failed build leaves no image, in bulk or in
# the home: what remains is valid. /var/log/allbulk.log ends with
# the reports of /var/log/doitall_cnt_kvm.log and
# /var/log/doitall_cnt_kvm_demo.log, then what was delivered.
#
#   ./allbulk.sh --go        hours: erase, build every container, base VM,
#                            desktop and demo image, deliver, report
#   ./allbulk.sh --deliver   no build: deliver what bulk holds, here
#   ./allbulk.sh             this text and the names, no build
#
#   cnt_kvm/doitall.sh --all       -> ~/cnt_bulk/<img>.tar.gz
#                                     ~/kvm_bulk/<img>.qcow2.gz
#   cnt_kvm_demo/doitall.sh --all  -> ~/cnt_demo_bulk/<img>.tar.gz
#                                     ~/kvm_demo_bulk/<img>.qcow2.gz
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
# shellcheck source-path=SCRIPTDIR source=doitall_common.sh
. "${HERE}/doitall_common.sh"
DC_TAG="allbulk"
ABLOG="/var/log/allbulk.log"
DEST="/home/${USER}"
CNT_BULK="${DEST}/cnt_bulk"
KVM_BULK="${DEST}/kvm_bulk"
CNT_DEMO_BULK="${DEST}/cnt_demo_bulk"
KVM_DEMO_BULK="${DEST}/kvm_demo_bulk"
GZIP=$(command -v pigz || command -v gzip)
NOTDEL=()
FAILED=()
DELIVERED=0
RAN=()
AB_OUT=""
STOPPED=0

# The names behind one label of a doitall --list, "label : a b c".
listed() { "${HERE}/$1/doitall.sh" --list | sed -n "s/^$2 *: *//p"; }

names_cnt_kvm() {
  listed cnt_kvm "base images"
  listed cnt_kvm "containers only"
  listed cnt_kvm "desktops" | tr ':' '_'
}

names_cnt_kvm_demo() { listed cnt_kvm_demo "images"; }

# deliver <cnt dest> <kvm dest> <name>...: each image of <name> that bulk
# holds and that is newer than STAMP; one it does not hold is told.
deliver() {
  local cdest="$1" kdest="$2" name src found
  shift 2
  mkdir -p "${cdest}" "${kdest}" || { log "ERROR: ${cdest} or ${kdest} cannot be made"; NOTDEL+=("$@"); return 1; }
  for name in "$@"; do
    found=0
    src="${BULK}/${name}.tar.gz"
    if [ -f "${src}" ] && [ "${src}" -nt "${STAMP}" ]; then
      found=1
      if cp "${src}" "${cdest}/${name}.tar.gz.tmp" && \
         mv "${cdest}/${name}.tar.gz.tmp" "${cdest}/${name}.tar.gz"; then
        log "DELIVERED ${name}: ${cdest}/${name}.tar.gz"
        DELIVERED=$((DELIVERED + 1))
      else
        rm -f "${cdest}/${name}.tar.gz.tmp"
        log "ERROR ${name}: ${src} could not be copied into ${cdest}"
        NOTDEL+=("${name}")
      fi
    fi
    src="${BULK}/${name}.qcow2"
    if [ -f "${src}" ] && [ "${src}" -nt "${STAMP}" ]; then
      found=1
      if "${GZIP}" -c "${src}" > "${kdest}/${name}.qcow2.gz.tmp" && \
         mv "${kdest}/${name}.qcow2.gz.tmp" "${kdest}/${name}.qcow2.gz"; then
        log "DELIVERED ${name}: ${kdest}/${name}.qcow2.gz"
        DELIVERED=$((DELIVERED + 1))
      else
        rm -f "${kdest}/${name}.qcow2.gz.tmp"
        log "ERROR ${name}: ${src} could not be gzipped into ${kdest}"
        NOTDEL+=("${name}")
      fi
    fi
    if [ "${found}" = "0" ]; then
      NOTDEL+=("${name}")
      if [ -e "${BULK}/${name}.tar.gz" ] || [ -e "${BULK}/${name}.qcow2" ]; then
        log "NOT DELIVERED ${name}: its image in ${BULK} is older than this run"
      else
        log "NOT DELIVERED ${name}: neither ${name}.tar.gz nor ${name}.qcow2 in ${BULK}"
      fi
    fi
  done
}

# ab_erase <cnt dest> <kvm dest> <name>...: every image of <name>, in bulk
# and delivered.
ab_erase() {
  local cdest="$1" kdest="$2" name f
  shift 2
  DOITALL_ERASE_KVM=1
  for name in "$@"; do
    { dc_erase cnt "${name}" && dc_erase kvm "${name}"; } 2>&1 | prefix allbulk
    for f in "${cdest}/${name}.tar.gz" "${kdest}/${name}.qcow2.gz"; do
      [ -e "${f}" ] || continue
      if rm -f "${f}"; then log "erased ${f}"; else log "WARNING: ${f} could not be erased"; fi
    done
  done
}

# ab_run <dir> <cnt dest> <kvm dest> <name>...: the doitall of <dir> to its
# end, then what it built delivered.
ab_run() {
  local dir="$1" cdest="$2" kdest="$3" rc
  shift 3
  RAN+=("${dir}")
  log "RUN ${dir}: every image, followed with: tail -f /var/log/doitall_${dir}.log"
  AB_OUT=$(mktemp)
  "${HERE}/${dir}/doitall.sh" --all --wait > "${AB_OUT}" 2>&1
  rc=$?
  if ! grep -q -F '[doitall] DONE:' "/var/log/doitall_${dir}.log"; then
    log "ERROR ${dir}: its run ended without a report, its launch said:"
    grep -v -E '^[0-9-]+ [0-9:]+ \[' "${AB_OUT}" | prefix "${dir}"
  fi
  rm -f "${AB_OUT}"
  AB_OUT=""
  log "RUN ${dir}: ended with rc=${rc}"
  ab_failed "${dir}"
  deliver "${cdest}" "${kdest}" "$@"
}

# The failed builds the DONE line of the doitall log of <dir> names.
ab_failed() {
  # shellcheck disable=SC2207
  FAILED+=($(sed -n 's/^.*\[doitall\] DONE: ok: .*; failed: //p' "/var/log/doitall_$1.log" \
             | tail -n 1 | sed 's/^none$//'))
}

ab_report() {
  local dir
  log "REPORT"
  for dir in "${RAN[@]}"; do
    sed -n '/\[doitall\] REPORT:/,/\[doitall\] DONE:/p' "/var/log/doitall_${dir}.log" \
      | sed "s/\[doitall\]/[${dir}]/"
  done
  log "DELIVERED: ${DELIVERED} images into ${DEST}/{cnt,kvm,cnt_demo,kvm_demo}_bulk"
  [ "${#FAILED[@]}" = "0" ] || log "FAILED: ${FAILED[*]}, erased, not delivered"
  [ "${#NOTDEL[@]}" = "0" ] || log "NOT DELIVERED: ${NOTDEL[*]}"
  if [ "${STOPPED}" = "1" ]; then
    log "DONE: stopped before its end, ${#FAILED[@]} builds failed, not run: $(printf '%s\n' cnt_kvm cnt_kvm_demo | grep -v -x -F -f <(printf '%s\n' "${RAN[@]}") | tr '\n' ' ')"
  elif [ "${#FAILED[@]}" = "0" ] && [ "${#NOTDEL[@]}" = "0" ]; then
    log "DONE: every image renewed and delivered"
  else
    log "DONE: ${#FAILED[@]} builds failed, ${#NOTDEL[@]} names without an image, the reports above say why"
  fi
}

# A stop also stops the doitall session this run started.
ab_stop() {
  local pid=""
  trap '' TERM INT HUP
  log "STOP: signal received"
  STOPPED=1
  [ -n "${AB_OUT}" ] && pid=$(grep -o -E 'running in session [0-9]+' "${AB_OUT}" | grep -o -E '[0-9]+$')
  if [ -n "${pid}" ]; then
    kill -TERM -- "-${pid}"
    # Its report is written when it ends.
    for _ in $(seq 1 60); do kill -0 "${pid}" 2>/dev/null || break; sleep 1; done
    ab_failed "${RAN[-1]}"
  fi
  kill -TERM 0
  wait
  ab_report
  rm -f "${STAMP}" "${AB_OUT}"
  rm_copy
  exit 1
}

ab_daemon() {
  local names demo
  trap ab_stop TERM INT HUP
  # Built after this, an image is of this run.
  STAMP=$(mktemp)
  names=$(names_cnt_kvm)
  demo=$(names_cnt_kvm_demo)
  log "LAUNCH session $$ from ${DC_ORIGIN}, run from its copy ${DC_COPY}"
  log "ERASE: every image to be rebuilt, in ${BULK} and in ${DEST}"
  # shellcheck disable=SC2086
  ab_erase "${CNT_BULK}" "${KVM_BULK}" ${names}
  # shellcheck disable=SC2086
  ab_erase "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}" ${demo}
  # shellcheck disable=SC2086
  ab_run cnt_kvm "${CNT_BULK}" "${KVM_BULK}" ${names}
  # shellcheck disable=SC2086
  ab_run cnt_kvm_demo "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}" ${demo}
  ab_report
  rm -f "${STAMP}"
  rm_copy
  exit $(( ${#FAILED[@]} + ${#NOTDEL[@]} > 0 ))
}

[ -n "${ALLBULK_DAEMON:-}" ] && ab_daemon

NAMES=$(names_cnt_kvm)
DEMO_NAMES=$(names_cnt_kvm_demo)
if [ -z "${NAMES}" ] || [ -z "${DEMO_NAMES}" ]; then
  echo "allbulk.sh: a doitall.sh --list gave no name"
  exit 1
fi
case "${1:-}" in
  --go)
    # Nothing is erased unless it can be built again: this machine ready and
    # every file the builders read served, check_www_links.sh linking it.
    dc_preflight || { echo "nothing erased."; exit 1; }
    sudo "${HERE}/get_distro/check_www_links.sh" | grep -v -E '^OK '
    if [ "${PIPESTATUS[0]}" != 0 ]; then
      echo "FATAL: nothing erased, check_www_links.sh above says what is not served"
      exit 1
    fi
    # The detached run has no terminal for sudo: the logs are made now.
    for f in /var/log/doitall_cnt_kvm.log /var/log/doitall_cnt_kvm_demo.log; do
      dc_logfile "${f}" || exit 1
    done
    dc_detach "allbulk" "${ABLOG}" "/tmp/allbulk.lock" "allbulk.sh" ALLBULK_DAEMON=1
    exit 0 ;;
  --deliver)
    STAMP=$(mktemp)
    touch -d @0 "${STAMP}"
    # shellcheck disable=SC2086
    deliver "${CNT_BULK}" "${KVM_BULK}" ${NAMES}
    # shellcheck disable=SC2086
    deliver "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}" ${DEMO_NAMES}
    rm -f "${STAMP}"
    log "DELIVERED: ${DELIVERED} images"
    [ "${#NOTDEL[@]}" = "0" ] || { log "NOT DELIVERED: ${NOTDEL[*]}"; exit 1; }
    exit 0 ;;
  *)
    sed -n '2,/^set -u/{/^set -u/d;s/^# \{0,1\}//p}' "$0"
    echo "cnt_kvm      :" ${NAMES}
    echo "cnt_kvm_demo :" ${DEMO_NAMES}
    exit 0 ;;
esac
