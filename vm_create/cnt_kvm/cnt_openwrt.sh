#!/bin/bash
# OpenWrt container rootfs, from the mirrored rootfs tarball of the newest
# point release of the mirrored branch.
OPENWRT0="/var/lib/cloonix/bulk/openwrt"
WK="${OPENWRT0}"
# chroot lives in /usr/sbin, which is not on a normal user's PATH.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=$(command -v chroot 2>/dev/null)
[ -n "${CHROOT}" ] && [ -x "${CHROOT}" ] || { echo "ERROR: chroot not found"; exit 1; }
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
. "$(dirname "$0")/openwrt_common.sh"
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare openssl tar sha256sum; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  [ -e ${OPENWRT0} ] || [ -e ${OPENWRT0}.tar.gz ] && { echo "ERROR ${OPENWRT0} or ${OPENWRT0}.tar.gz exists, please erase"; exit 1; }
  wget -q --spider "${OPENWRT_TARGET}/sha256sums" || \
    { echo "ERROR: ${OPENWRT_TARGET}/sha256sums not reachable"
      echo "       run get_openwrt.sh to mirror openwrt,"
      echo "       then check_www_links.sh to publish it"
      exit 1; }
  mkdir -p "$(dirname ${OPENWRT0})"
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
set -eE
trap 'echo "ERROR: line ${LINENO}: ${BASH_COMMAND} failed"' ERR
openwrt_tree "${WK}"
openwrt_add "${WK}" rsyslog strace tcpdump ip-full vim file
for f in rsyslogd strace tcpdump vim file; do
  ${CHROOT} ${WK} sh -c "${CPATH}; command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, the install did not run"; exit 1; }
done
HASH=$(openssl passwd -6 root)
sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
grep -q "^root:${HASH}:" ${WK}/etc/shadow || { echo "ERROR: no root line in /etc/shadow"; exit 1; }
# Point the container at the mirror address reachable from inside cloonix.
openwrt_feeds "${WK}" "${OPENWRT_CNT_REPO}"
openwrt_untree "${WK}"
# /var is /tmp: a container has no procd to make run and lock at boot.
mkdir -p ${WK}/tmp/run ${WK}/tmp/lock
for i in "dev" "proc" "sys"; do chmod 755 ${WK}/${i}; done
chmod 700 ${WK}/root
chown -R --no-dereference 0:0 ${WK}
chmod -R u+w ${WK}
# rsyslog, locale, hosts and the accounts: written into the image here, not by
# cloonixsbininit at every boot.
. "$(dirname "$0")/cnt_common.sh"
cnt_common_config "${WK}" || exit 1
echo "rootfs ready in ${WK}"
cnt_common_pack "${WK}" || exit 1
