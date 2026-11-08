#!/bin/bash
# OpenWrt from the local mirror: the files of the point release its
# sha256sums names, apk reading the mirror only, signatures checked.
# The builders set CHROOT and CPATH.

OPENWRT_REPO="http://127.0.0.1/openwrt"
OPENWRT_CNT_REPO="http://10.117.0.1/openwrt"
OPENWRT_TARGET="${OPENWRT_REPO}/targets/x86/64"

# openwrt_file <suffix>: name of the mirrored openwrt-<release>-x86-64-<suffix>.
openwrt_file() {
  local suffix="$1" sums names
  sums=$(wget -q -O- "${OPENWRT_TARGET}/sha256sums") || \
    { echo "ERROR: ${OPENWRT_TARGET}/sha256sums not reachable" >&2; return 1; }
  names=$(printf '%s\n' "${sums}" | awk -v s="-x86-64-${suffix}" '
    { n = $2; sub(/^\*/, "", n) }
    n ~ /^openwrt-[0-9]/ && substr(n, length(n) - length(s) + 1) == s { print n }')
  if [ -z "${names}" ] || [ "$(printf '%s\n' "${names}" | wc -l)" != "1" ]; then
    echo "ERROR: ${OPENWRT_TARGET}/sha256sums names no single openwrt-*-x86-64-${suffix}" >&2
    return 1
  fi
  printf '%s\n' "${names}"
}

# openwrt_fetch <name> <dest>: download, checked against sha256sums.
openwrt_fetch() {
  local name="$1" dest="$2" want got
  want=$(wget -q -O- "${OPENWRT_TARGET}/sha256sums" | awk -v n="${name}" '
    { f = $2; sub(/^\*/, "", f) } f == n { print $1 }')
  [ -n "${want}" ] || { echo "ERROR: ${name} is not in ${OPENWRT_TARGET}/sha256sums"; return 1; }
  wget -q -O "${dest}" "${OPENWRT_TARGET}/${name}" || \
    { rm -f "${dest}"; echo "ERROR: could not fetch ${OPENWRT_TARGET}/${name}"; return 1; }
  got=$(sha256sum "${dest}" | cut -d' ' -f1)
  [ "${got}" = "${want}" ] || \
    { rm -f "${dest}"; echo "ERROR: ${name}: sha256 ${got}, the mirror says ${want}"; return 1; }
}

# openwrt_feeds <root> <base>: every apk feed of <root> read from <base>,
# which mirrors the layout of downloads.openwrt.org/releases/<release>.
openwrt_feeds() {
  local root="$1" base="$2" f lists=() bad rc=0
  for f in "${root}"/etc/apk/repositories.d/*.list "${root}/etc/apk/repositories"; do
    [ -f "${f}" ] && lists+=("${f}")
  done
  [ -s "${root}/etc/apk/repositories.d/distfeeds.list" ] || \
    { echo "ERROR: ${root}/etc/apk/repositories.d/distfeeds.list missing"; return 1; }
  sed -i -E -e "s#^(\# )?[a-z]+://[^ ]*/releases/[^/]+/#\1${base}/#" \
            -e "s#^(\# )?(${OPENWRT_REPO}|${OPENWRT_CNT_REPO})/#\1${base}/#" "${lists[@]}"
  # No line selected, grep exits 1: the nominal case.
  bad=$(grep -h -v -E "^[[:space:]]*(#|$)|^${base}/" "${lists[@]}") || rc=$?
  [ "${rc}" -le 1 ] || { echo "FATAL: the apk feed lists of ${root} could not be read"; return 1; }
  if [ -n "${bad}" ]; then
    echo "FATAL: these apk feeds are not ${base}:"
    printf '%s\n' "${bad}" | sed 's/^/         /'
    return 1
  fi
  echo "apk feeds: ${base} only"
}

# openwrt_tree <root>: the mirrored rootfs, its apk on the mirror.
openwrt_tree() {
  local root="$1" name tgz d
  name=$(openwrt_file rootfs.tar.gz) || return 1
  tgz=$(mktemp --suffix=.tar.gz)
  openwrt_fetch "${name}" "${tgz}" || return 1
  mkdir -p "${root}"
  # Device nodes cannot be made in a user namespace.
  tar -xzf "${tgz}" -C "${root}" --exclude='./dev/*' || \
    { rm -f "${tgz}"; echo "ERROR: could not unpack ${name}"; return 1; }
  rm -f "${tgz}"
  [ -x "${root}/usr/bin/apk" ] || { echo "ERROR: ${name} holds no /usr/bin/apk"; return 1; }
  echo "openwrt: ${name}"
  # /var is /tmp, filled by procd at boot: its init scripts lock in /var/lock.
  mkdir -p "${root}/tmp/lock" "${root}/tmp/run"
  mkdir -p "${root}"/{dev,proc,sys}
  for d in dev sys proc; do
    mountpoint -q "${root}/${d}" || mount --rbind "/${d}" "${root}/${d}" || return 1
  done
  openwrt_feeds "${root}" "${OPENWRT_REPO}" || return 1
  ${CHROOT} "${root}" sh -c "${CPATH}; apk update" || { echo "ERROR: apk update failed"; return 1; }
}

# openwrt_add <root> <package>...: installed from the mirror the way OpenWrt
# builds its images: with IPKG_INSTROOT, which --preserve-env passes to the
# post-install scripts, they enable services without starting them, procd
# and ubus being absent, and leave uci-defaults to the first boot.
openwrt_add() {
  local root="$1"; shift
  ${CHROOT} "${root}" sh -c "${CPATH}; IPKG_INSTROOT=/ apk add --preserve-env $*" || \
    { echo "ERROR: apk add $* failed"; return 1; }
}

# openwrt_untree <root>: pseudo filesystems detached, runtime leftovers out;
# /var is a link to /tmp in OpenWrt.
openwrt_untree() {
  local root="$1" d
  for d in dev proc sys; do
    if mountpoint -q "${root}/${d}"; then umount -l "${root}/${d}" || return 1; fi
  done
  rm -rf "${root:?}"/dev/* "${root}"/proc/* "${root}"/sys/* "${root}"/tmp/*
}
