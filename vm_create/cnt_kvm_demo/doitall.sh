#!/bin/bash
# Build the images the demos of demos/ ask for, container and kvm, WITHOUT
# sudo.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
KVMDIR="${HERE}/../cnt_kvm"
BULK="/var/lib/cloonix/bulk"
LOGDIR="/tmp/doitall-demo-logs"
IMAGES="frr_trixie bird_trixie babel_trixie batman_trixie ovn_trixie"
IMAGES="${IMAGES} dns_trixie unbound_trixie knot_trixie dhcp_trixie"
IMAGES="${IMAGES} ntp_trixie wg_trixie strongswan_trixie libreswan_trixie"
IMAGES="${IMAGES} openvpn_trixie tinc_trixie proxy_trixie squid_trixie"
IMAGES="${IMAGES} smtp_trixie mqtt_trixie asterisk_alpine nft_trixie"
IMAGES="${IMAGES} suricata_trixie ebpf_trixie radius_trixie vrrp_trixie"
IMAGES="${IMAGES} cluster_trixie perf_trixie flow_trixie arp_trixie"
# The kvm images a demo needs beyond the plain distributions.
KVM_IMAGES="python_trixie"
ALL_IMAGES="${IMAGES} ${KVM_IMAGES}"
# A demo name from demos/ may be given instead of an image name.
DEMOMAP="frr=frr_trixie bird=bird_trixie babel=babel_trixie"
DEMOMAP="${DEMOMAP} batman=batman_trixie ovn=ovn_trixie dns=dns_trixie"
DEMOMAP="${DEMOMAP} unbound=unbound_trixie,knot_trixie"
DEMOMAP="${DEMOMAP} knot=knot_trixie,unbound_trixie dhcp=dhcp_trixie"
DEMOMAP="${DEMOMAP} ntp=ntp_trixie wg=wg_trixie"
DEMOMAP="${DEMOMAP} ipsec=strongswan_trixie,libreswan_trixie"
DEMOMAP="${DEMOMAP} openvpn=openvpn_trixie tinc=tinc_trixie"
DEMOMAP="${DEMOMAP} proxy=proxy_trixie squid=squid_trixie smtp=smtp_trixie"
DEMOMAP="${DEMOMAP} mqtt=mqtt_trixie asterisk=asterisk_alpine"
DEMOMAP="${DEMOMAP} nftables=nft_trixie suricata=suricata_trixie"
DEMOMAP="${DEMOMAP} ipv6=frr_trixie isis=frr_trixie tls=openvpn_trixie"
DEMOMAP="${DEMOMAP} vxlan=frr_trixie arp=arp_trixie"
DEMOMAP="${DEMOMAP} ebpf=ebpf_trixie radius=radius_trixie"
DEMOMAP="${DEMOMAP} vrrp=vrrp_trixie cluster=cluster_trixie perf=perf_trixie bigflows=flow_trixie"
DEMOMAP="${DEMOMAP} realnet=python_trixie phy=python_trixie"
usage() {
  cat <<EOF
doitall.sh -- build the demo container images, without sudo.

Usage: ./doitall.sh --all
       ./doitall.sh --allcnt
       ./doitall.sh --allkvm
       ./doitall.sh <name> [name ...]
       ./doitall.sh --list

  --all              every image below. This is the whole set the demos
                     of demos/ need, and takes a while
  --allcnt           every container image below, and no qcow2: the
                     kvm images (${KVM_IMAGES}) are left untouched
  --allkvm           the kvm images only (${KVM_IMAGES}), erased and
                     rebuilt, no container touched
  --list             print what would be built, build nothing
  -h, --help         this text

A <name> is either an image name or a demo name from demos/. The demo name
is usually the one you want, and asks for every image that demo needs:

  ./doitall.sh ipsec           strongswan_trixie and libreswan_trixie
  ./doitall.sh frr             frr_trixie
  ./doitall.sh dns dhcp        those two images
  ./doitall.sh frr_trixie      the image, named directly

With no argument at all this help is printed and nothing is built.

The distributions -- plain trixie, alpine, rocky and the rest, container
and VM -- are not here. They are in ../cnt_kvm/doitall.sh, and none of
them is needed to build anything here.

Images : ${ALL_IMAGES}

Logs go to ${LOGDIR}/cnt_<name>.log as well as to the terminal.
EOF
}
if [ "$#" = "0" ]; then
  usage
  exit 0
fi
DO_ALL=0
SELECTED=""
for arg in "$@"; do
  case "${arg}" in
    --all|-all|all)   DO_ALL=1 ;;
    --allcnt|-allcnt|allcnt)
                      DO_ALL=2 ;;
    --allkvm|-allkvm|allkvm)
                      DO_ALL=3 ;;
    --list|-list|list)
      echo "images : ${ALL_IMAGES}"
      echo "demos  : $(for e in ${DEMOMAP}; do printf "%s " "${e%%=*}"; done)"
      exit 0 ;;
    -h|--help|-help|help)
      usage
      exit 0 ;;
    -*) echo "unknown option: ${arg}"; echo "try: $0 --help"; exit 1 ;;
    *)  SELECTED="${SELECTED} ${arg}" ;;
  esac
done
if [ "${DO_ALL}" = "0" ]; then
  if [ -z "${SELECTED}" ]; then
    echo "nothing named, and no --all. Nothing to build."
    echo "try: $0 --help"
    exit 1
  fi
  resolved=""
  for s in ${SELECTED}; do
    hit=""
    for e in ${DEMOMAP}; do
      if [ "${e%%=*}" = "${s}" ]; then
        hit=$(echo "${e#*=}" | tr ',' ' ')
        break
      fi
    done
    if [ -z "${hit}" ]; then
      case " ${ALL_IMAGES} " in
        *" ${s} "*) hit="${s}" ;;
        *) echo "FATAL: '${s}' is neither an image nor a demo here."
           echo "try: $0 --list"
           exit 1 ;;
      esac
    fi
    for h in ${hit}; do
      case " ${resolved} " in
        *" ${h} "*) : ;;
        *) resolved="${resolved} ${h}" ;;
      esac
    done
  done
  IMAGES="${resolved}"
elif [ "${DO_ALL}" = "1" ]; then
  IMAGES="${ALL_IMAGES}"
elif [ "${DO_ALL}" = "3" ]; then
  IMAGES="${KVM_IMAGES}"
fi
# Refuse to run as root.
if [ "$(id -u)" = "0" ]; then
  echo "FATAL: do not run this as root."
  echo "       Every builder here works unprivileged, and a root-owned"
  echo "       tree in ${BULK} would block later runs."
  exit 1
fi
fail=0
if [ ! -f "${KVMDIR}/cnt_common.sh" ]; then
  echo "MISSING: ${KVMDIR}/cnt_common.sh"
  echo "         Every builder here sources it. It lives in cnt_kvm and is"
  echo "         shared, not copied."
  fail=1
fi
script_of() {
  if [ -x "${HERE}/cnt_$1.sh" ]; then echo "cnt_$1.sh"; else echo "kvm_$1.sh"; fi
}

for d in ${IMAGES}; do
  [ -x "${HERE}/$(script_of ${d})" ] || \
    { echo "MISSING: ${HERE}/$(script_of ${d}) (not present or not executable)"; fail=1; }
done
for d in ${IMAGES}; do
  s="${HERE}/$(script_of ${d})"
  [ -f "${s}" ] || continue
  if grep -qE '(^|[;&|]|\$\(|`|^[[:space:]]*(then|else|do)[[:space:]]+)[[:space:]]*sudo[[:space:]]' "${s}"; then
    echo "FATAL: $(script_of ${d}) calls sudo:"
    grep -nE '(^|[;&|]|\$\(|`|^[[:space:]]*(then|else|do)[[:space:]]+)[[:space:]]*sudo[[:space:]]' "${s}" \
      | head -3 | sed 's/^/         /'
    echo "       This script builds without root; convert it to the"
    echo "       unshare approach the other builders use."
    fail=1
  fi
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
for d in ${IMAGES}; do
  t="${BULK}/${d}"
  case "${d}" in
    ""|*/*|.|..) echo "FATAL: refusing to erase for image '${d}'"; exit 1 ;;
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
  # A kvm image is rebuilt from scratch, the previous one is not kept.
  for q in "${t}.qcow2" "${t}.qcow2.prev"; do
    [ -e "${q}" ] || continue
    echo "erasing previous VM image ${q}"
    rm -f "${q}"
    [ -e "${q}" ] && { echo "FATAL: could not erase ${q}"; exit 1; }
  done
  if [ -d "${t}_targz" ]; then
    echo "erasing previous extracted tree ${t}_targz"
    chmod -R u+w "${t}_targz" 2>/dev/null || true
    rm -rf "${t}_targz" 2>/dev/null || true
    [ -e "${t}_targz" ] && echo "WARNING ${t}_targz belongs to another user and stays: a net using it keeps the old image"
  fi
done
START=$(date +%s)
WARNED=""
echo "======================================================================"
echo " doitall demo:${IMAGES}"
echo "   logs       : ${LOGDIR}"
echo "   started    : $(date '+%H:%M:%S')"
echo "======================================================================"
run() {
  local d="$1"
  local kind
  kind=$(script_of "${d}")
  local script="${HERE}/${kind}"
  local log="${LOGDIR}/${kind%.sh}.log"
  local t0 t1
  echo
  echo "----------------------------------------------------------------------"
  echo " ${kind%.sh}   ($(date '+%H:%M:%S'))"
  echo "----------------------------------------------------------------------"
  t0=$(date +%s)
  # The builder's exit status matters, not tee's.
  local rcfile="${LOGDIR}/.rc_${kind%.sh}"
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
    echo " FAILED: ${kind} (exit ${rc}) after $((t1-t0))s"
    echo " log:    ${log}"
    echo
    echo " Stopping here so this can be corrected. Nothing after this"
    echo " point has been built. Re-run with:"
    echo "./doitall.sh ${d}"
    echo " to retry just this one once the cause is fixed."
    echo "======================================================================"
    exit "${rc}"
  fi
  # A cnt builder delivers only the tar.gz: a tree left in bulk is a failure.
  case "${kind}" in
    cnt_*)
      if [ ! -f "${BULK}/${d}.tar.gz" ] || [ -e "${BULK}/${d}" ]; then
        echo
        echo "======================================================================"
        echo " FAILED: ${kind} exited 0 but did not deliver ${BULK}/${d}.tar.gz"
        [ -e "${BULK}/${d}" ] && echo "         ${BULK}/${d} is still there: the pack step did not run"
        echo " log:    ${log}"
        echo "======================================================================"
        exit 1
      fi ;;
  esac
  # What a builder went on without is told again at the very end.
  local w
  w=$(grep -h '^ *WARNING' "${log}" | sed 's/^ */    /' | sort -u)
  [ -n "${w}" ] && WARNED="${WARNED}
  ${kind}
${w}"
  echo " ok: ${kind%.sh} in $((t1-t0))s"
}
for d in ${IMAGES}; do run "${d}"; done
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
for d in ${IMAGES}; do
  if [ -f "${BULK}/${d}.tar.gz" ]; then
    printf "  cnt %-18s %s\n" "${d}.tar.gz" "$(du -sh "${BULK}/${d}.tar.gz" 2>/dev/null | cut -f1)"
  elif [ -f "${BULK}/${d}.qcow2" ]; then
    printf "  kvm %-18s %s\n" "${d}.qcow2" "$(du -sh "${BULK}/${d}.qcow2" 2>/dev/null | cut -f1)"
  fi
done
echo "======================================================================"
