#!/bin/bash
# Build every image of cnt_kvm and of cnt_kvm_demo, then deliver them, the
# containers as they are, the qcow2 gzipped, into four directories of the
# home. Stops at the first error.
#
#   ./allbulk.sh --go        hours: every container, base VM, desktop, demo
#   ./allbulk.sh --deliver   no build: deliver what bulk holds. After a --go
#                            that stopped on a builder, once that builder was
#                            corrected and re-run alone with its doitall.sh
#   ./allbulk.sh             this text and what would be delivered, no build
#
#   cnt_kvm/doitall.sh --all       -> ~/cnt_bulk/<img>.tar.gz
#                                     ~/kvm_bulk/<img>.qcow2.gz
#   cnt_kvm_demo/doitall.sh --all  -> ~/cnt_demo_bulk/<img>.tar.gz
#                                     ~/kvm_demo_bulk/<img>.qcow2.gz
#
# The images stay in bulk. What is delivered is what each doitall lists, not
# what bulk happens to hold, and with --go only if this run built it.
set -Eeuo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BULK="/var/lib/cloonix/bulk"
DEST="/home/${USER}"
CNT_BULK="${DEST}/cnt_bulk"
KVM_BULK="${DEST}/kvm_bulk"
CNT_DEMO_BULK="${DEST}/cnt_demo_bulk"
KVM_DEMO_BULK="${DEST}/kvm_demo_bulk"
GZIP=$(command -v pigz || command -v gzip)

trap 'echo "allbulk.sh: stopped on an error, line ${LINENO}" >&2' ERR

# The names behind one label of a doitall --list, "label : a b c".
listed() {
  local dir="$1" label="$2"
  "${HERE}/${dir}/doitall.sh" --list --all |
    sed -n "s/^${label} *: *//p"
}

# base and container-only names, then the desktops "distro:kind" as the
# image name distro_kind
names_cnt_kvm() {
  listed cnt_kvm "base images"
  listed cnt_kvm "containers only"
  listed cnt_kvm "desktops" | tr ':' '_'
}

names_cnt_kvm_demo() {
  listed cnt_kvm_demo "images"
}

# Deliver each listed image by what it is in bulk: a tar.gz, a qcow2, or
# both. A name with neither, or an image older than this run, is an error.
deliver() {
  local cnt_dest="$1" kvm_dest="$2" name src found
  shift 2
  for name in "$@"; do
    found=0
    src="${BULK}/${name}.tar.gz"
    if [ -f "${src}" ]; then
      fresh "${src}"
      echo "  ${src} -> ${cnt_dest}/"
      cp "${src}" "${cnt_dest}/${name}.tar.gz.tmp"
      mv "${cnt_dest}/${name}.tar.gz.tmp" "${cnt_dest}/${name}.tar.gz"
      found=1
    fi
    src="${BULK}/${name}.qcow2"
    if [ -f "${src}" ]; then
      fresh "${src}"
      echo "  ${src} -> ${kvm_dest}/${name}.qcow2.gz"
      "${GZIP}" -c "${src}" > "${kvm_dest}/${name}.qcow2.gz.tmp"
      mv "${kvm_dest}/${name}.qcow2.gz.tmp" "${kvm_dest}/${name}.qcow2.gz"
      found=1
    fi
    if [ "${found}" = "0" ]; then
      echo "allbulk.sh: ${name} is listed and is neither ${name}.tar.gz" \
           "nor ${name}.qcow2 in ${BULK}" >&2
      exit 1
    fi
  done
}

fresh() {
  if [ ! "$1" -nt "${STAMP}" ]; then
    echo "allbulk.sh: $1 was not built by this run" >&2
    exit 1
  fi
}

NAMES=$(names_cnt_kvm)
DEMO_NAMES=$(names_cnt_kvm_demo)
if [ -z "${NAMES}" ] || [ -z "${DEMO_NAMES}" ]; then
  echo "allbulk.sh: a doitall.sh --list gave no name" >&2
  exit 1
fi

if [ "${1:-}" != "--go" ] && [ "${1:-}" != "--deliver" ]; then
  sed -n '2,/^set -Eeuo/{/^set -Eeuo/d;s/^# \{0,1\}//p}' "$0"
  echo "cnt_kvm      :" ${NAMES}
  echo "cnt_kvm_demo :" ${DEMO_NAMES}
  exit 0
fi

mkdir -p "${CNT_BULK}" "${KVM_BULK}" "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}"
STAMP=$(mktemp)
trap 'rm -f "${STAMP}"' EXIT

if [ "$1" = "--deliver" ]; then
  touch -d @0 "${STAMP}"            # nothing was built: every image is fresh
else
  cd "${HERE}/cnt_kvm"
  ./doitall.sh --all
fi
deliver "${CNT_BULK}" "${KVM_BULK}" ${NAMES}

if [ "$1" = "--go" ]; then
  cd "${HERE}/cnt_kvm_demo"
  ./doitall.sh --all
fi
deliver "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}" ${DEMO_NAMES}

echo
echo "delivered:"
du -sh "${CNT_BULK}" "${KVM_BULK}" "${CNT_DEMO_BULK}" "${KVM_DEMO_BULK}"
