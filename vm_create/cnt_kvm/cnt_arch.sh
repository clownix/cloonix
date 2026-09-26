#!/bin/bash
# Arch Linux container rootfs (directory, no kernel/grub).
ARCH0="/var/lib/cloonix/bulk/arch"
WK="${ARCH0}"
BSTRAP="/tmp/arch-cnt-bootstrap"
TARBALL="/tmp/arch-cnt-bootstrap-$(id -u).tar.zst"
MIRROR="http://127.0.0.1/arch"
CNTMIRROR="http://10.117.0.1/arch"
BOOTSTRAP_URL="${MIRROR}/iso/latest/archlinux-bootstrap-x86_64.tar.zst"
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=$(command -v chroot 2>/dev/null)
[ -n "${CHROOT}" ] && [ -x "${CHROOT}" ] || { echo "ERROR: chroot not found"; exit 1; }
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare tar zstd openssl; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  [ -e ${ARCH0} ] || [ -e ${ARCH0}.tar.gz ] && { echo "ERROR ${ARCH0} or ${ARCH0}.tar.gz exists, please erase"; exit 1; }
  wget -q --spider --timeout=20 --tries=2 \
      ${MIRROR}/core/os/x86_64/ 1>/dev/null 2>&1 || \
    { echo "${MIRROR}/core/os/x86_64 not reachable"; exit 1; }
  echo "REPO OK ${MIRROR}/core/os/x86_64"
  mkdir -p $(dirname ${ARCH0}) 2>/dev/null || true
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
# uid 0 inside a private user+mount namespace from here on.
set -e
BROOT="${BSTRAP}/root.x86_64"
mkdir -p ${WK}/{dev,proc,sys,tmp}
rm -rf ${BSTRAP} && mkdir -p ${BSTRAP}
if [ -s ${TARBALL} ]; then
  echo "reusing cached ${TARBALL}"
elif wget -q --timeout=30 --tries=2 -O ${TARBALL} "${BOOTSTRAP_URL}"; then
  echo "fetched bootstrap from ${BOOTSTRAP_URL}"
else
  rm -f ${TARBALL}
  echo "ERROR: ${BOOTSTRAP_URL} is not served by the local mirror."
  echo "       Run vm_create/get_distro/get_arch.sh, which mirrors"
  echo "       iso/latest/ for exactly this, then check_www_links.sh."
  exit 1
fi
tar --warning=no-unknown-keyword \
    --use-compress-program=unzstd -xf ${TARBALL} -C ${BSTRAP}
[ -x ${BROOT}/usr/bin/pacman ] || { echo "ERROR: bootstrap has no pacman"; exit 1; }
# Point the bootstrap env at the mirror.
echo "Server = ${MIRROR}/\$repo/os/\$arch" > ${BROOT}/etc/pacman.d/mirrorlist
sed -i 's/^SigLevel.*/SigLevel = Never/' ${BROOT}/etc/pacman.conf
sed -i 's/^ *LocalFileSigLevel.*/LocalFileSigLevel = Never/' ${BROOT}/etc/pacman.conf
sed -i 's/^ *CheckSpace/#CheckSpace/' ${BROOT}/etc/pacman.conf
cp -f /etc/resolv.conf ${BROOT}/etc/resolv.conf 2>/dev/null || true
for d in dev sys proc; do
  mkdir -p ${BROOT}/${d}
  mountpoint -q ${BROOT}/${d} || mount --rbind /${d} ${BROOT}/${d} 2>/dev/null || \
    echo "WARNING: could not mount ${BROOT}/${d}"
done
# Bind the target tree inside the bootstrap env so pacstrap/pacman can reach
# it.
mkdir -p ${BROOT}/mnt/target
mount --bind ${WK} ${BROOT}/mnt/target
for d in dev proc sys; do
  mkdir -p ${BROOT}/mnt/target/${d}
  mountpoint -q ${BROOT}/mnt/target/${d} || \
    mount --rbind /${d} ${BROOT}/mnt/target/${d} 2>/dev/null || \
    echo "WARNING: could not pre-mount ${d} in the target"
done
PKGS="base openssh sudo which iproute2 net-tools tcpdump strace syslog-ng "
PKGS+="qemu-guest-agent iputils dhcpcd iptables e2fsprogs"
# Resolve the list and check the mirror serves it, before any of it is
# downloaded.
. "$(dirname "$0")/arch_common.sh"
arch_check_pkgs_served "${CHROOT}" "${BROOT}" /mnt/target "${PKGS}" || exit 1
# NOT -K, and under a timeout.
set +e
TMO=""
command -v timeout >/dev/null 2>&1 && TMO="timeout --foreground 900"
${TMO} ${CHROOT} ${BROOT} sh -c "${CPATH}; pacstrap -G /mnt/target ${PKGS}"
PACRC=$?
set -e
[ ${PACRC} = 124 ] && echo "note: pacstrap timed out after 15 minutes"

if [ ${PACRC} -ne 0 ]; then
  echo "note: pacstrap failed (rc=${PACRC}), falling back to pacman --root"
  echo "      arch-chroot cannot mount /proc in a user namespace; pacman"
  echo "      with --root does the same install without a chroot."
  set +e
  ${CHROOT} ${BROOT} sh -c "${CPATH}; pacman --root /mnt/target \
      --cachedir /var/cache/pacman/pkg \
      --noconfirm --needed -Sy ${PKGS}"
  PMRC=$?
  set -e
  if [ ${PMRC} -ne 0 ]; then
    echo "ERROR: pacman --root also failed (rc=${PMRC})"
    echo
    echo "       Read WHICH error pacman gave, they mean different things:"
    echo
    echo "       \"target not found: <pkg>\" - the databases downloaded and"
    echo "       parsed fine and the name is simply not in core or extra."
    echo "       Nothing is wrong with the mirror; the package list in this"
    echo "       script is. Arch is rolling and drops packages: rsyslog left"
    echo "       for the AUR in 2018, which is why this list says syslog-ng."
    echo "       Check the name at archlinux.org/packages and fix PKGS."
    echo
    echo "       \"failed retrieving file ... 404\" - the database names a"
    echo "       package the mirror does not have. If the missing names carry"
    echo "       an epoch (a '1:' in them, e.g. nftables-1:1.1.6), the mirror"
    echo "       filesystem cannot store them: exFAT and NTFS do not allow"
    echo "       ':' in filenames. Host the arch mirror on ext4, or let"
    echo "       get_arch.sh apply its colon-free rewrite. On ordinary names"
    echo "       the mirror is skewed instead - re-run get_arch.sh, which"
    echo "       uses rsync --delay-updates for exactly this reason."
    exit 1
  fi
fi
for d in dev proc sys; do umount -l ${BROOT}/mnt/target/${d} 2>/dev/null || true; done
umount ${BROOT}/mnt/target 2>/dev/null || true
for d in dev proc sys; do umount -l ${BROOT}/${d} 2>/dev/null || true; done
if [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: no usable rootfs in ${WK}"
  exit 1
fi
# Sanity: the tools the container needs must be present.
mkdir -p ${WK}/{dev,proc,sys}
for d in dev sys proc; do
  mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d} 2>/dev/null || true
done
for f in ip tcpdump; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, install did not run"; exit 1; }
done
# Mirror for the finished rootfs, locale, root password.
mkdir -p ${WK}/etc/pacman.d
echo "Server = ${CNTMIRROR}/\$repo/os/\$arch" > ${WK}/etc/pacman.d/mirrorlist
sed -i 's/^SigLevel.*/SigLevel = Never/' ${WK}/etc/pacman.conf 2>/dev/null || true
grep -q '^en_US.UTF-8' ${WK}/etc/locale.gen 2>/dev/null || \
  echo "en_US.UTF-8 UTF-8" >> ${WK}/etc/locale.gen
${CHROOT} ${WK} sh -c "${CPATH}; locale-gen" || echo "WARNING: locale-gen failed"
echo "LANG=en_US.UTF-8" > ${WK}/etc/locale.conf
echo "arch" > ${WK}/etc/hostname
HASH=$(openssl passwd -6 root)
if [ -f ${WK}/etc/shadow ] && grep -q '^root:' ${WK}/etc/shadow; then
  sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
else
  echo "root:${HASH}:20000:0:99999:7:::" > ${WK}/etc/shadow
  chmod 600 ${WK}/etc/shadow
fi
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/*
rm -rf ${WK}/var/cache/pacman/pkg/* 2>/dev/null || true
for i in "dev" "proc" "sys"; do chmod 755 ${WK}/${i}; done
chmod 700 ${WK}/root
for d in dev proc sys run; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
set +e
chown -R --no-dereference 0:0 ${WK}
chmod -R u+w ${WK}
set -e
# rsyslog, locale, hosts and the accounts: written into the image here, not by
# cloonixsbininit at every boot.
. "$(dirname "$0")/cnt_common.sh"
cnt_common_config "${WK}" || exit 1
echo "rootfs ready in ${WK}"
cnt_common_pack "${WK}" || exit 1
