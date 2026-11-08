#!/bin/bash
# Build the images the demos of demos/ ask for, container and kvm, WITHOUT
# sudo, in a detached run that tries every build: ../doitall_common.sh.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
KVMDIR="${HERE}/../cnt_kvm"
# shellcheck source-path=SCRIPTDIR source=../doitall_common.sh
. "${HERE}/../doitall_common.sh"
[ -n "${DOITALL_DAEMON:-}" ] && dc_daemon
IMAGES="frr_trixie bird_trixie babel_trixie batman_trixie ovn_trixie"
IMAGES="${IMAGES} dns_trixie unbound_trixie knot_trixie dhcp_trixie"
IMAGES="${IMAGES} ntp_trixie wg_trixie strongswan_trixie libreswan_trixie"
IMAGES="${IMAGES} openvpn_trixie tinc_trixie proxy_trixie squid_trixie"
IMAGES="${IMAGES} smtp_trixie mqtt_trixie asterisk_trixie nft_trixie"
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
DEMOMAP="${DEMOMAP} mqtt=mqtt_trixie asterisk=asterisk_trixie"
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
       ./doitall.sh --all --wait

  --all              every image below. This is the whole set the demos
                     of demos/ need, and takes a while
  --allcnt           every container image below, and no qcow2: the
                     kvm images (${KVM_IMAGES}) are left untouched
  --allkvm           the kvm images only (${KVM_IMAGES}), erased and
                     rebuilt, no container touched
  --list             print what would be built, build nothing
  --wait             follow the run here, return its status
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

The run is detached: it goes on without the terminal, from a copy of
vm_create made at launch, and tries every build even after a failure. Its
whole output, then a report of the failures, of the warnings the ok builds
went on with and of the images, go to ${LOG}:

  tail -f ${LOG}

With --wait it is followed here and its status returned, as allbulk.sh
uses it. A doitall of the other directory waits for this one to end.
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
    --wait|-wait|wait) DOITALL_WAIT=1 ;;
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
if [ ! -f "${KVMDIR}/cnt_common.sh" ]; then
  echo "MISSING: ${KVMDIR}/cnt_common.sh"
  echo "         Every builder here sources it. It lives in cnt_kvm and is"
  echo "         shared, not copied."
  exit 1
fi
BUILDS=""
for d in ${IMAGES}; do
  if [ -e "${HERE}/cnt_${d}.sh" ]; then BUILDS="${BUILDS} cnt:${d}"; else BUILDS="${BUILDS} kvm:${d}"; fi
done
DOITALL_BUILDS="${BUILDS# }"
# A kvm image here is rebuilt from scratch, the previous one is not kept.
DOITALL_ERASE_KVM=1
dc_launch
