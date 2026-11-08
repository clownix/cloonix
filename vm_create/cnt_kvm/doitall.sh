#!/bin/bash
# Build the distro images, containers and VMs, WITHOUT sudo, in a detached
# run that tries every build: ../doitall_common.sh.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
# shellcheck source-path=SCRIPTDIR source=../doitall_common.sh
. "${HERE}/../doitall_common.sh"
[ -n "${DOITALL_DAEMON:-}" ] && dc_daemon
DISTROS="alpine trixie resolute rocky fedora44 fedora45 arch tumbleweed openwrt"
DESKTOPS="alpine:xfce trixie:lxde trixie:gnome resolute:gnome"
DESKTOPS="${DESKTOPS} rocky:gnome fedora45:gnome arch:plasma tumbleweed:plasma"
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
  --wait             follow the run here, return its status
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

The run is detached: it goes on without the terminal, from a copy of
vm_create made at launch, and tries every build even after a failure. Its
whole output, then a report of the failures, of the warnings the ok builds
went on with and of the images, go to ${LOG}:

  tail -f ${LOG}

With --wait it is followed here and its status returned, as allbulk.sh
uses it. A doitall of the other directory waits for this one to end.
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
    --wait|-wait|wait)  DOITALL_WAIT=1 ;;
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
for d in ${DISTROS}; do
  case " ${ALL_DISTROS} ${ALL_CNT_ONLY} " in
    *" ${d} "*) : ;;
    *) echo "FATAL: unknown distro '${d}'"
       [ "${d}" = "freebsd" ] && \
         echo "       freebsd uses the internet, so it is not built here: ./kvm_freebsd.sh"
       exit 1 ;;
  esac
done
BUILDS=""
if [ "${DO_CNT}" = "1" ]; then
  for d in ${DISTROS} ${CNT_ONLY}; do BUILDS="${BUILDS} cnt:${d}"; done
fi
if [ "${DO_KVM}" = "1" ]; then
  for d in ${DISTROS}; do BUILDS="${BUILDS} kvm:${d}"; done
fi
# Desktops last: each starts from the base image of its distro.
if [ "${DO_DESK}" = "1" ]; then
  for e in ${DESKTOPS}; do
    case " ${DISTROS} " in
      *" ${e%%:*} "*) BUILDS="${BUILDS} kvm:${e%%:*}_${e##*:}:${e%%:*}" ;;
    esac
  done
fi
DOITALL_BUILDS="${BUILDS# }"
DOITALL_ERASE_KVM="${ERASE_KVM}"
dc_launch
