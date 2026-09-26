#!/bin/bash
# Build the distro images, containers and VMs, WITHOUT sudo.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
BULK="/var/lib/cloonix/bulk"
LOGDIR="/tmp/doitall-logs-$(id -u)"
DISTROS="alpine trixie resolute rocky fedora44 arch tumbleweed"
DESKTOPS="alpine:xfce trixie:lxde trixie:gnome resolute:gnome"
DESKTOPS="${DESKTOPS} rocky:gnome fedora44:gnome arch:plasma tumbleweed:plasma"
CNT_ONLY="trixie_systemd"
ALL_DISTROS="${DISTROS}"
ALL_CNT_ONLY="${CNT_ONLY}"
usage() {
  cat <<EOF
doitall.sh -- build the cloonix container and VM images, without sudo.

Usage: ./doitall.sh <what> [distro ...]

What to build. The flags ADD UP, so asking for two kinds builds both:

  --cnt              every container image, and nothing else
  --allcnt           same as --cnt: every container, no qcow2 touched
  --allkvm           every base VM image, no container touched. Unlike
                     --kvm, the previous qcow2 and its .prev are erased
                     first rather than kept as .prev
  --kvm              every base VM image, and nothing else. No desktop
                     is built by this: see --desktop below
  --cnt --kvm        every container AND every base VM, still no desktop.
                     This is the full working set, and what you want when
                     you want everything that is used day to day
  --desktop          the desktop VMs only. Each is built FROM the base VM
                     image of its distro, so that base must already exist
  --all              same as --cnt --kvm --desktop
  --list             print what would be built, build nothing
  -h, --help         this text

Narrowing to some distros. Add names after the flags and only those are
built. A name is taken from whichever list holds it, so a container-only
image asks for no VM of itself:

  ./doitall.sh --cnt --kvm trixie rocky    both kinds, those two distros
  ./doitall.sh --cnt frr_trixie            that one container
  ./doitall.sh --kvm trixie                that one base VM
  ./doitall.sh --desktop trixie            the trixie desktops

With no argument at all this help is printed and nothing is built: a full
run costs hours and should not be startable by a mistyped command.

Base images     : ${ALL_DISTROS}
Containers only : ${ALL_CNT_ONLY}
Desktops        : ${DESKTOPS}

Everything here is built from the local mirror, never from the internet.
freebsd has no local mirror, so it is not built here:  ./kvm_freebsd.sh

The images the demos ask for -- frr, asterisk, strongswan and the rest --
are not here. They are in ../cnt_kvm_demo:  cd ../cnt_kvm_demo && ./doitall.sh --all

Logs go to ${LOGDIR}/<kind>_<name>.log as well as to the terminal.
EOF
}
# Nothing at all means the user has not said what to build.
if [ "$#" = "0" ]; then
  usage
  exit 0
fi
DO_CNT=0
DO_KVM=0
DO_DESK=0
ERASE_KVM=0
GOT_KIND=0
SELECTED=""
for arg in "$@"; do
  case "${arg}" in
    --cnt|-cnt|cnt|--allcnt|-allcnt|allcnt)
                        DO_CNT=1; GOT_KIND=1 ;;
    --kvm|-kvm|kvm)     DO_KVM=1; GOT_KIND=1 ;;
    --allkvm|-allkvm|allkvm)
                        DO_KVM=1; ERASE_KVM=1; GOT_KIND=1 ;;
    --desktop|-desktop|desktop)
                        DO_DESK=1; GOT_KIND=1 ;;
    --all|-all|all)     DO_CNT=1; DO_KVM=1; DO_DESK=1; GOT_KIND=1 ;;
    --list|-list|list)
      echo "base images     : ${ALL_DISTROS}"
      echo "containers only : ${ALL_CNT_ONLY}"
      echo "desktops        : ${DESKTOPS}"
      exit 0 ;;
    -h|--help|-help|help)
      usage
      exit 0 ;;
    -*) echo "unknown option: ${arg}"; echo "try: $0 --help"; exit 1 ;;
    *)  SELECTED="${SELECTED} ${arg}" ;;
  esac
done
if [ "${GOT_KIND}" = "0" ]; then
  DO_CNT=1
  DO_KVM=1
fi
# A selection narrows both lists; a name builds whichever list holds it.
if [ -n "${SELECTED}" ]; then
  new_distros=""
  new_cnt_only=""
  for s in ${SELECTED}; do
    case " ${CNT_ONLY} " in
      *" ${s} "*) new_cnt_only="${new_cnt_only} ${s}"; continue ;;
    esac
    new_distros="${new_distros} ${s}"
  done
  DISTROS="${new_distros}"
  CNT_ONLY="${new_cnt_only}"
fi
# A selection can leave nothing at all to do: "--kvm frr_trixie" names a
# container-only image, so the kvm list ends up empty.
todo=0
[ "${DO_CNT}" = "1" ] && [ -n "${DISTROS}${CNT_ONLY}" ] && todo=1
[ "${DO_KVM}" = "1" ] && [ -n "${DISTROS}" ] && todo=1
if [ "${DO_DESK}" = "1" ]; then
  for e in ${DESKTOPS}; do
    case " ${DISTROS} " in
      *" ${e%%:*} "*) todo=1 ;;
    esac
  done
fi
if [ "${todo}" = "0" ]; then
  echo "nothing to build for that combination."
  echo "  containers : $([ "${DO_CNT}" = "1" ] && echo yes || echo no)"
  echo "  base VMs   : $([ "${DO_KVM}" = "1" ] && echo yes || echo no)"
  echo "  desktops   : $([ "${DO_DESK}" = "1" ] && echo yes || echo no)"
  echo "  names      :${SELECTED:- (none, so all)}"
  echo "try: $0 --list"
  exit 1
fi
# Refuse to run as root.
if [ "$(id -u)" = "0" ]; then
  echo "FATAL: do not run this as root."
  echo "       Every builder here works unprivileged, and a root-owned"
  echo "       tree in ${BULK} would block later runs."
  exit 1
fi
fail=0
for d in ${DISTROS}; do
  case " ${ALL_DISTROS} ${ALL_CNT_ONLY} " in
    *" ${d} "*) : ;;
    *) echo "FATAL: unknown distro '${d}'"
       [ "${d}" = "freebsd" ] && \
         echo "       freebsd uses the internet, so it is not built here: ./kvm_freebsd.sh"
       exit 1 ;;
  esac
  [ "${DO_CNT}" = "1" ] && [ ! -x "${HERE}/cnt_${d}.sh" ] && \
    { echo "MISSING: ${HERE}/cnt_${d}.sh (not present or not executable)"; fail=1; }
  [ "${DO_KVM}" = "1" ] && [ ! -x "${HERE}/kvm_${d}.sh" ] && \
    { echo "MISSING: ${HERE}/kvm_${d}.sh (not present or not executable)"; fail=1; }
done
for d in ${CNT_ONLY}; do
  [ "${DO_CNT}" = "1" ] && [ ! -x "${HERE}/cnt_${d}.sh" ] && \
    { echo "MISSING: ${HERE}/cnt_${d}.sh (not present or not executable)"; fail=1; }
done
if [ "${DO_DESK}" = "1" ]; then
  for e in ${DESKTOPS}; do
    d="${e%%:*}"
    k="${e##*:}"
    case " ${DISTROS} " in
      *" ${d} "*) : ;;
      *) continue ;;
    esac
    [ ! -x "${HERE}/kvm_${d}_${k}.sh" ] && \
      { echo "MISSING: ${HERE}/kvm_${d}_${k}.sh"; fail=1; }
    if [ "${DO_KVM}" != "1" ] && [ ! -f "${BULK}/${d}.qcow2" ]; then
      echo "MISSING: ${BULK}/${d}.qcow2 — build the base first: ./doitall.sh --kvm ${d}"
      fail=1
    fi
  done
fi
for d in ${DISTROS} ${CNT_ONLY}; do
  for kind in cnt kvm; do
    [ "${kind}" = "cnt" ] && [ "${DO_CNT}" = "0" ] && continue
    [ "${kind}" = "kvm" ] && [ "${DO_KVM}" = "0" ] && continue
    s="${HERE}/${kind}_${d}.sh"
    [ -f "${s}" ] || continue
    if grep -qE '(^|[;&|]|\$\(|`|^[[:space:]]*(then|else|do)[[:space:]]+)[[:space:]]*sudo[[:space:]]' "${s}"; then
      echo "FATAL: ${kind}_${d}.sh calls sudo:"
      grep -nE '(^|[;&|]|\$\(|`|^[[:space:]]*(then|else|do)[[:space:]]+)[[:space:]]*sudo[[:space:]]' "${s}" \
        | head -3 | sed 's/^/         /'
      echo "       This script builds without root; convert it to the"
      echo "       unshare approach the other builders use, or drop"
      echo "       ${d} from the list."
      fail=1
    fi
  done
done
[ "${fail}" = "0" ] || { echo; echo "pre-flight failed, nothing was built."; exit 1; }
[ -d "${BULK}" ] || { echo "FATAL: ${BULK} does not exist"; exit 1; }
[ -w "${BULK}" ] || { echo "FATAL: ${BULK} is not writable by $(id -un)"; exit 1; }
for t in unshare flock mkfs.ext4 qemu-img; do
  command -v "$t" >/dev/null 2>&1 || [ -x "/usr/sbin/$t" ] || \
    { echo "FATAL: $t not found"; exit 1; }
done
mkdir -p "${LOGDIR}"
rm -f "${LOGDIR}"/*.log
if [ "${DO_CNT}" = "1" ]; then
  for d in ${DISTROS} ${CNT_ONLY}; do
    t="${BULK}/${d}"
    case "${d}" in
      ""|*/*|.|..) echo "FATAL: refusing to erase for distro '${d}'"; exit 1 ;;
    esac
    case "${t}" in
      "${BULK}"/?*) : ;;
      *) echo "FATAL: refusing to erase '${t}' (not under ${BULK})"; exit 1 ;;
    esac
    if [ -e "${t}" ]; then
      echo "erasing previous container tree ${t}"
      if unshare --map-root-user --user --map-auto -- \
           /bin/sh -c "rm -rf '${t}'" 2>/dev/null && [ ! -e "${t}" ]; then
        :
      else
        chmod -R u+w "${t}" 2>/dev/null || true
        rm -rf "${t}" 2>/dev/null || true
      fi
      [ -e "${t}" ] && { echo "FATAL: could not erase ${t}"; exit 1; }
    fi
    # The delivered form, and what a net extracted from a previous one.
    rm -f "${t}.tar.gz"
    if [ -d "${t}_targz" ]; then
      echo "erasing previous extracted tree ${t}_targz"
      chmod -R u+w "${t}_targz" 2>/dev/null || true
      rm -rf "${t}_targz" 2>/dev/null || true
      [ -e "${t}_targz" ] && echo "WARNING ${t}_targz belongs to another user and stays: a net using it keeps the old image"
    fi
  done
fi
if [ "${ERASE_KVM}" = "1" ]; then
  for d in ${DISTROS}; do
    case "${d}" in
      ""|*/*|.|..) echo "FATAL: refusing to erase for distro '${d}'"; exit 1 ;;
    esac
    for q in "${BULK}/${d}.qcow2" "${BULK}/${d}.qcow2.prev"; do
      [ -e "${q}" ] || continue
      echo "erasing previous VM image ${q}"
      rm -f "${q}"
      [ -e "${q}" ] && { echo "FATAL: could not erase ${q}"; exit 1; }
    done
  done
fi
START=$(date +%s)
WARNED=""
echo "======================================================================"
echo " doitall: ${DISTROS}$([ "${DO_CNT}" = "1" ] && echo "${CNT_ONLY:+ +${CNT_ONLY}}")"
echo "   containers : $([ "${DO_CNT}" = "1" ] && echo yes || echo no)"
echo "   base VMs   : $([ "${DO_KVM}" = "1" ] && echo yes || echo no)"
echo "   desktops   : $([ "${DO_DESK}" = "1" ] && echo yes || echo no)"
echo "   logs       : ${LOGDIR}"
echo "   started    : $(date '+%H:%M:%S')"
echo "======================================================================"
run() {
  local kind="$1" d="$2" hint="$3"
  local script="${HERE}/${kind}_${d}.sh"
  local log="${LOGDIR}/${kind}_${d}.log"
  local t0 t1
  echo
  echo "----------------------------------------------------------------------"
  echo " ${kind} ${d}   ($(date '+%H:%M:%S'))"
  echo "----------------------------------------------------------------------"
  t0=$(date +%s)
  # The builder's exit status matters, not tee's.
  local rcfile="${LOGDIR}/.rc_${kind}_${d}"
  rm -f "${rcfile}"
  (
    cd "${HERE}" || exit 127
    "${script}"
    echo $? > "${rcfile}"
  ) 2>&1 | tee "${log}"
  local rc
  rc=$(cat "${rcfile}" 2>/dev/null || echo 127)
  rm -f "${rcfile}"
  t1=$(date +%s)
  if [ "${rc}" != "0" ]; then
    echo
    echo "======================================================================"
    echo " FAILED: ${kind}_${d}.sh (exit ${rc}) after $((t1-t0))s"
    echo " log:    ${log}"
    echo
    echo " Stopping here so this can be corrected. Nothing after this"
    echo " point has been built. Re-run with:"
    echo "./doitall.sh ${hint}"
    echo " to retry just this one once the cause is fixed."
    echo "======================================================================"
    exit "${rc}"
  fi
  # A cnt builder delivers only the tar.gz: a tree left in bulk is a failure.
  if [ "${kind}" = "cnt" ]; then
    if [ ! -f "${BULK}/${d}.tar.gz" ] || [ -e "${BULK}/${d}" ]; then
      echo
      echo "======================================================================"
      echo " FAILED: cnt_${d}.sh exited 0 but did not deliver ${BULK}/${d}.tar.gz"
      [ -e "${BULK}/${d}" ] && echo "         ${BULK}/${d} is still there: the pack step did not run"
      echo " log:    ${log}"
      echo "======================================================================"
      exit 1
    fi
  fi
  # What a builder went on without is told again at the very end.
  local w
  w=$(grep -h '^ *WARNING' "${log}" | sed 's/^ */    /' | sort -u)
  [ -n "${w}" ] && WARNED="${WARNED}
  ${kind}_${d}.sh
${w}"
  echo " ok: ${kind} ${d} in $((t1-t0))s"
}
if [ "${DO_CNT}" = "1" ]; then
  for d in ${DISTROS}; do run cnt "${d}" "--cnt ${d}"; done
  for d in ${CNT_ONLY}; do run cnt "${d}" "--cnt ${d}"; done
fi
if [ "${DO_KVM}" = "1" ]; then
  for d in ${DISTROS}; do run kvm "${d}" "--kvm ${d}"; done
fi
# Desktops last: each copies the base image of its distro.
if [ "${DO_DESK}" = "1" ]; then
  for e in ${DESKTOPS}; do
    d="${e%%:*}"
    k="${e##*:}"
    case " ${DISTROS} " in
      *" ${d} "*) run kvm "${d}_${k}" "--desktop ${d}" ;;
    esac
  done
fi
END=$(date +%s)
echo
echo "======================================================================"
if [ -n "${WARNED}" ]; then
  echo " ALL BUILT, WITH WARNINGS  (total $(( (END-START)/60 ))m $(( (END-START)%60 ))s)"
  echo "${WARNED}"
else
  echo " ALL OK, no warning  (total $(( (END-START)/60 ))m $(( (END-START)%60 ))s)"
fi
echo "======================================================================"
for d in ${DISTROS}; do
  if [ "${DO_CNT}" = "1" ] && [ -f "${BULK}/${d}.tar.gz" ]; then
    printf "  cnt %-12s %s\n" "${d}.tar.gz" "$(du -sh "${BULK}/${d}.tar.gz" 2>/dev/null | cut -f1)"
  fi
  if [ "${DO_KVM}" = "1" ] && [ -f "${BULK}/${d}.qcow2" ]; then
    printf "  kvm %-12s %s\n" "${d}" "$(du -sh "${BULK}/${d}.qcow2" 2>/dev/null | cut -f1)"
  fi
done
for d in ${CNT_ONLY}; do
  if [ "${DO_CNT}" = "1" ] && [ -f "${BULK}/${d}.tar.gz" ]; then
    printf "  cnt %-12s %s\n" "${d}.tar.gz" "$(du -sh "${BULK}/${d}.tar.gz" 2>/dev/null | cut -f1)"
  fi
done
if [ "${DO_DESK}" = "1" ]; then
  for e in ${DESKTOPS}; do
    d="${e%%:*}"
    k="${e##*:}"
    if [ -f "${BULK}/${d}_${k}.qcow2" ]; then
      printf "  kvm %-12s %s\n" "${d}_${k}" \
             "$(du -sh "${BULK}/${d}_${k}.qcow2" 2>/dev/null | cut -f1)"
    fi
  done
fi
echo "======================================================================"
