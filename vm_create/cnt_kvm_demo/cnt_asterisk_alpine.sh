#!/bin/bash
# asterisk_alpine: ../cnt_kvm/cnt_alpine.sh with asterisk and a SIP toolbox
# added, and nothing else.
ALPINE0="/var/lib/cloonix/bulk/asterisk_alpine"
WK="${ALPINE0}"
REPO="http://127.0.0.1/alpine"
CNTREPO="http://10.117.0.1/alpine"
ARCH="x86_64"
MINIROOT_URL="${REPO}/releases/${ARCH}/alpine-minirootfs-latest-${ARCH}.tar.gz"
MINIROOT="/tmp/alpine-minirootfs-${ARCH}.tar.gz"
# chroot lives in /usr/sbin, which is not on a normal user's PATH.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=$(command -v chroot 2>/dev/null)
[ -n "${CHROOT}" ] && [ -x "${CHROOT}" ] || { echo "ERROR: chroot not found"; exit 1; }
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
# NOTE: --map-auto needs /etc/subuid and /etc/subgid entries for you, e.g.
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare openssl tar; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  [ -e ${ALPINE0} ] || [ -e ${ALPINE0}.tar.gz ] && { echo "ERROR ${ALPINE0} or ${ALPINE0}.tar.gz exists, please erase"; exit 1; }
  wget -q --spider ${REPO}/main/${ARCH}/ || \
    { echo "${REPO}/main/${ARCH} not reachable"; exit 1; }
  wget -q --spider ${REPO}/community/${ARCH}/ || \
    { echo "${REPO}/community/${ARCH} not reachable (sngrep lives there)"; exit 1; }
  wget -q --spider "${MINIROOT_URL}" || \
    { echo "ERROR: ${MINIROOT_URL} not reachable"
      echo "       run get_alpine.sh to mirror the minirootfs tarball,"
      echo "       then setup_www_links.sh to publish it"
      exit 1; }
  mkdir -p $(dirname ${ALPINE0}) 2>/dev/null || true
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
set -e
mkdir -p ${WK}
# Cached locally so a retry does not refetch it.
if [ ! -s ${MINIROOT} ]; then
  wget -q -O ${MINIROOT} "${MINIROOT_URL}" || \
    { rm -f ${MINIROOT}; echo "ERROR: could not fetch ${MINIROOT_URL}"; exit 1; }
fi
tar xf ${MINIROOT} -C ${WK}
if [ ! -x ${WK}/sbin/apk ] && [ ! -x ${WK}/usr/bin/apk ]; then
  echo "ERROR: minirootfs did not unpack a usable apk in ${WK}"
  exit 1
fi
mkdir -p ${WK}/{dev,proc,sys}
for d in dev sys proc; do
  mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d} 2>/dev/null || \
    echo "WARNING: could not mount ${WK}/${d}"
done
# apk appends /<arch> itself when it fetches APKINDEX.tar.gz, so these lines
# must stop at the repo name.
cat > ${WK}/etc/apk/repositories << EOF
${REPO}/main
${REPO}/community
EOF
cp -f /etc/resolv.conf ${WK}/etc/resolv.conf 2>/dev/null || true
APK="apk --allow-untrusted"
${CHROOT} ${WK} sh -c "${CPATH}; ${APK} update"
for i in musl musl-utils coreutils musl-locales util-linux tzdata \
         qemu-guest-agent libcap openrc alpine-base kbd kbd-bkeymaps \
         file stress-ng strace rsyslog; do
  ${CHROOT} ${WK} sh -c "${CPATH}; ${APK} --no-cache --update add ${i}" || \
    { echo "ERROR: apk add ${i} failed"; exit 1; }
done
# The PBX.
${CHROOT} ${WK} sh -c "${CPATH}; ${APK} --no-cache --update add asterisk" || \
  { echo "ERROR: apk add asterisk failed"; exit 1; }
if [ ! -x ${WK}/usr/sbin/asterisk ]; then
  echo "ERROR: asterisk is not in ${WK}, the mirror does not carry it"
  exit 1
fi
GOT=""
MISSING=""
set +e
for p in asterisk-sample-config asterisk-openrc asterisk-sound-en \
         asterisk-sounds-en asterisk-moh-opsound-wav asterisk-opus \
         sipp sngrep sipsak tcpdump sox baresip python3; do
  if ${CHROOT} ${WK} sh -c "${CPATH}; ${APK} --no-cache --update add ${p}" \
       1>/dev/null 2>&1; then
    GOT="${GOT} ${p}"
  else
    MISSING="${MISSING} ${p}"
  fi
done
set -e
# Sanity check: the chroot steps must have produced the tools asked for.
for f in strace file asterisk; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, install did not run"; exit 1; }
done
for i in killprocs mount-ro savecache; do
  ${CHROOT} ${WK} sh -c "${CPATH}; rc-update add ${i} shutdown" || \
    echo "WARNING: rc-update add ${i} shutdown failed"
done
for i in hostname hwclock loopback procfs sysctl devfs sysfs dmesg \
         mdev qemu-guest-agent cgroups; do
  ${CHROOT} ${WK} sh -c "${CPATH}; rc-update add ${i} boot" || \
    echo "WARNING: rc-update add ${i} boot failed"
done
# cloonix talks to the guest over a virtserialport named
# org.qemu.guest_agent.0, so the agent MUST be in a runlevel.
if ls ${WK}/etc/runlevels/*/qemu-guest-agent >/dev/null 2>&1; then
  echo "qemu-guest-agent: enabled"
else
  echo "WARNING: qemu-guest-agent is in no runlevel"
fi
${CHROOT} ${WK} sh -c "${CPATH}; setup-hostname asterisk" 2>/dev/null || \
  echo "asterisk" > ${WK}/etc/hostname
# passwd(1) needs PAM and real root, so write the hash directly.
HASH=$(openssl passwd -6 root)
if [ -f ${WK}/etc/shadow ] && grep -q '^root:' ${WK}/etc/shadow; then
  sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
else
  echo "root:${HASH}:20000:0:99999:7:::" > ${WK}/etc/shadow
  chmod 640 ${WK}/etc/shadow
fi
[ -e ${WK}/bin/bbsuid ] && chmod +r ${WK}/bin/bbsuid
# Point the container at the mirror address reachable from inside cloonix.
cat > ${WK}/etc/apk/repositories << EOF
${CNTREPO}/main
${CNTREPO}/community
EOF
# Detach the pseudo filesystems before touching them, otherwise rm and chmod
# operate on the host's mountpoints and fail with EPERM.
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/*
rm -rf ${WK}/var/cache/apk/* 2>/dev/null || true
for i in "dev" "proc" "sys"; do chmod 755 ${WK}/${i}; done
chmod 700 ${WK}/root
for d in dev proc sys run; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
set +e
chown -R --no-dereference 0:0 ${WK}
chmod -R u+w ${WK}
for i in "/var/log/asterisk" "/var/lib/asterisk" "/var/spool/asterisk" \
         "/var/run/asterisk"; do
  [ -d "${WK}${i}" ] && chmod -R 1777 "${WK}${i}"
done
set -e
# rsyslog, locale, hosts and the accounts: written into the image here, not by
# cloonixsbininit at every boot.
. "$(dirname "$0")/../cnt_kvm/cnt_common.sh"
cnt_common_config "${WK}" || exit 1
echo "  asterisk_alpine: asterisk in place"
[ -n "${GOT}" ]     && echo "  asterisk_alpine: also installed -${GOT}"
[ -n "${MISSING}" ] && echo "  asterisk_alpine: NOT in the mirror, skipped -${MISSING}"
echo "rootfs ready in ${WK}"
cnt_common_pack "${WK}" || exit 1
exit 0
