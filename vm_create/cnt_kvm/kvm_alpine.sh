#!/bin/bash
# Alpine Linux qcow2 image, built WITHOUT root privileges.
DISTRO="alpine"
ROOTDIR="/var/lib/cloonix/bulk/raw_alpine_dir"
DISK="/var/lib/cloonix/bulk/alpine_disk.img"
QCOW2="/var/lib/cloonix/bulk/alpine.qcow2"
REPO="http://127.0.0.1/alpine"
CNTREPO="http://10.117.0.1/alpine"
ARCH="x86_64"
MINIROOT_URL="${REPO}/releases/${ARCH}/alpine-minirootfs-latest-${ARCH}.tar.gz"
MINIROOT="/tmp/alpine-minirootfs-${ARCH}-$(id -u).tar.gz"
# Disk geometry.
DISK_SIZE_G=32
P1_START=2048          # sectors
P1_SECTORS=2048        # 1 MiB BIOS boot partition (ef02)
P2_START=4096          # sectors
SECTOR=512
CORE_MAX=$((P1_SECTORS * SECTOR))
# sbin is not on a normal user's PATH on Debian hosts.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
SGDISK=/usr/sbin/sgdisk
[ -x ${SGDISK} ] || SGDISK=$(command -v sgdisk 2>/dev/null)
MKFS=/usr/sbin/mkfs.ext4
[ -x ${MKFS} ] || MKFS=$(command -v mkfs.ext4 2>/dev/null)
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare openssl qemu-img truncate dd od flock tar; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  for t in "${CHROOT}" "${SGDISK}" "${MKFS}"; do
    [ -n "$t" ] && [ -x "$t" ] || { echo "ERROR: missing $t"; exit 1; }
  done
  ${MKFS} -V 2>&1 | grep -qE 'mke2fs 1\.(4[3-9]|[5-9][0-9])' || \
    echo "WARNING: mkfs.ext4 may be too old for -d (need e2fsprogs >= 1.43)"
  wget -q --spider "${MINIROOT_URL}" || \
    { echo "ERROR: ${MINIROOT_URL} not reachable"
      echo "       run get_alpine.sh to mirror the minirootfs tarball,"
      echo "       then setup_www_links.sh to publish it"
      exit 1; }
  rm -f ${DISK} 2>/dev/null || true
  if [ -e ${ROOTDIR} ]; then
    if [ "${KEEP_TREE}" = "1" ]; then
      echo "KEEP_TREE=1: reusing the existing tree in ${ROOTDIR}"
    else
      echo "removing previous tree ${ROOTDIR}"
      # Built with --map-auto: apk chowns files to system accounts that land
      # as high subuids the login user cannot unlink.
      unshare --map-root-user --user --map-auto -- rm -rf ${ROOTDIR} 2>/dev/null || \
        rm -rf ${ROOTDIR} 2>/dev/null || true
      [ -e ${ROOTDIR} ] && { echo "ERROR: could not remove ${ROOTDIR}"; exit 1; }
    fi
  fi
  if [ -e ${QCOW2} ]; then
    echo "note: ${QCOW2} exists and will be replaced when the build succeeds"
  fi
  wget -q --spider ${REPO}/main/${ARCH}/ || \
    { echo "${REPO}/main/${ARCH} not reachable"; exit 1; }
  mkdir -p $(dirname ${ROOTDIR}) 2>/dev/null || true
  # These builders must NOT run concurrently.
  LOCKFILE=/var/lib/cloonix/bulk
  mkdir -p ${LOCKFILE} 2>/dev/null; exec 9<${LOCKFILE}
  if ! flock -n 9; then
    echo "ERROR: another cloonix image build is already running."
    echo "       These builders cannot run concurrently - wait for it to"
    echo "       finish (the lock is held on ${LOCKFILE})."
    exit 1
  fi
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
# uid 0 inside a private user+mount namespace from here on.
set -e
WK="${ROOTDIR}"
for d in tmp/disk.img tmp/grubdir dev proc sys; do
  umount -l ${WK}/${d} 2>/dev/null || true
done
rm -f ${WK}/tmp/disk.img 2>/dev/null || true
rmdir ${WK}/tmp/grubdir 2>/dev/null || true
# STEP 1: unpack the minirootfs.
if [ "${KEEP_TREE}" = "1" ] && { [ -x ${WK}/sbin/apk ] || [ -x ${WK}/usr/bin/apk ]; }; then
echo "KEEP_TREE=1: skipping STEP 1 (tree already built)"
mkdir -p ${WK}/{dev,proc,sys,tmp}
else
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
fi
mkdir -p ${WK}/{dev,proc,sys,tmp}
for d in dev sys proc; do
  if ! mountpoint -q ${WK}/${d}; then
    mount --rbind /${d} ${WK}/${d} 2>/dev/null || \
      echo "WARNING: could not mount ${WK}/${d}"
  fi
done
# apk appends /<arch> itself: stop at the repo name.
cat > ${WK}/etc/apk/repositories << EOF
${REPO}/main
${REPO}/community
EOF
cp -f /etc/resolv.conf ${WK}/etc/resolv.conf 2>/dev/null || true
if [ "${KEEP_TREE}" = "1" ] && { [ -x ${WK}/usr/sbin/grub-bios-setup ] || [ -x ${WK}/usr/lib/grub/i386-pc/grub-bios-setup ]; }; then
echo "KEEP_TREE=1: skipping STEP 2 (packages already installed)"
else
APK="apk --allow-untrusted"
${CHROOT} ${WK} sh -c "${CPATH}; ${APK} update"
# grub + grub-bios provide grub-mkimage and the i386-pc modules.
for p in musl musl-utils musl-locales tzdata linux-virt mkinitfs \
         grub grub-bios qemu-guest-agent openrc alpine-base kbd \
         kbd-bkeymaps dhcpcd openssh vim strace tcpdump iproute2 \
         net-tools e2fsprogs; do
  ${CHROOT} ${WK} sh -c "${CPATH}; ${APK} --no-cache add ${p}" || \
    echo "WARNING: apk add ${p} failed"
done
fi
# Sanity: the tools the rest of the script needs must be present.
for f in grub-mkimage; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing from the rootfs (grub package)"; exit 1; }
done
[ -d ${WK}/usr/lib/grub/i386-pc ] || \
  { echo "ERROR: /usr/lib/grub/i386-pc missing (grub-bios package)"; exit 1; }
# grub-bios-setup may be on PATH or only inside the module directory,
# depending on how the distro splits the grub packages.
if ${CHROOT} ${WK} sh -c "${CPATH} command -v grub-bios-setup >/dev/null 2>&1"; then
  BIOS_SETUP="grub-bios-setup"
elif [ -x ${WK}/usr/lib/grub/i386-pc/grub-bios-setup ]; then
  BIOS_SETUP="/usr/lib/grub/i386-pc/grub-bios-setup"
else
  echo "ERROR: grub-bios-setup not found, neither on PATH nor in"
  echo "       /usr/lib/grub/i386-pc/"
  exit 1
fi
echo "using bios-setup: ${BIOS_SETUP}"
mkdir -p ${WK}/etc/mkinitfs/modules.d
cat > ${WK}/etc/mkinitfs/modules.d/scsi << EOF
kernel/drivers/block/virtio*
kernel/drivers/virtio/virtio*
kernel/drivers/scsi/virtio*
kernel/drivers/net/virtio*
kernel/drivers/char/virtio*
EOF
cat >> ${WK}/etc/modules << EOF
virtio_console
virtio_scsi
virtio_pci
fuse
vhost-net
EOF
vmlinuz=$(cd ${WK}/boot && ls vmlinuz-* 2>/dev/null | tail -1)
[ -n "${vmlinuz}" ] || { echo "ERROR: no kernel in /boot (linux-virt)"; exit 1; }
version=${vmlinuz#vmlinuz-}
echo "kernel version: ${version}"
# mkinitfs names its output initramfs-<flavour>; linux-virt gives "virt".
${CHROOT} ${WK} sh -c "${CPATH}; mkinitfs -f /etc/mkinitfs/mkinitfs.conf ${version}" || \
${CHROOT} ${WK} sh -c "${CPATH}; mkinitfs ${version}" || \
  echo "WARNING: mkinitfs failed"
INITRD=$(cd ${WK}/boot && ls initramfs-* 2>/dev/null | tail -1)
[ -n "${INITRD}" ] || { echo "ERROR: no initramfs in /boot"; exit 1; }
echo "initramfs: ${INITRD}"
# STEP 4: filesystem UUID, chosen up front so grub.cfg and fstab can reference
# it before the filesystem exists.
FSUUID=$(cat /proc/sys/kernel/random/uuid)
echo "rootfs UUID: ${FSUUID}"
KERN="rw noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
# Alpine's initramfs needs modules= and root= on the kernel line.
KERN+=" modules=sd-mod,usb-storage,ext4,virtio_scsi,virtio_pci,virtio_blk"
KERN+=" rootfstype=ext4"
mkdir -p ${WK}/boot/grub
# root= is a UUID again.
cat > ${WK}/boot/grub/grub.cfg << EOF
set timeout=1
set default=0
insmod part_gpt
insmod ext2
insmod search_fs_uuid
insmod linux
search --no-floppy --fs-uuid --set=root ${FSUUID}
menuentry 'Alpine Linux' {
    linux /boot/${vmlinuz} root=UUID=${FSUUID} ${KERN}
    initrd /boot/${INITRD}
}
EOF
mkdir -p ${WK}/etc/default
printf "GRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "${KERN}" > ${WK}/etc/default/grub
# STEP 6: GRUB core image.
GDIR=${WK}/boot/grub/i386-pc
mkdir -p ${GDIR}
cp -f ${WK}/usr/lib/grub/i386-pc/*.img ${GDIR}/ 2>/dev/null || true
cp -f ${WK}/usr/lib/grub/i386-pc/*.mod ${GDIR}/ 2>/dev/null || true
${CHROOT} ${WK} sh -c "${CPATH} grub-mkimage -O i386-pc \
    -p '(hd0,gpt2)/boot/grub' -o /boot/grub/i386-pc/core.img \
    biosdisk part_gpt ext2 search search_fs_uuid normal linux echo \
    configfile boot minicmd"
CORE=${GDIR}/core.img
[ -s ${CORE} ] || { echo "ERROR: grub-mkimage produced nothing"; exit 1; }
CORE_SIZE=$(stat -c %s ${CORE})
echo "core.img: ${CORE_SIZE} bytes (limit ${CORE_MAX})"
[ ${CORE_SIZE} -le ${CORE_MAX} ] || \
  { echo "ERROR: core.img exceeds the BIOS boot partition"; exit 1; }
for f in boot.img diskboot.img; do
  [ -s ${GDIR}/${f} ] || { echo "ERROR: ${GDIR}/${f} missing"; exit 1; }
done
# STEP 7: fstab, root password, keymap.
cat > ${WK}/etc/fstab << EOF
UUID=${FSUUID}  /            ext4     defaults                      0 0
proc            /proc        proc     nosuid,noexec,nodev           0 0
sysfs           /sys         sysfs    nosuid,noexec,nodev           0 0
devpts          /dev/pts     devpts   nosuid,noexec,gid=5,mode=620  0 0
tmpfs           /run         tmpfs    defaults                      0 0
devtmpfs        /dev         devtmpfs mode=0755,nosuid              0 0
EOF
HASH=$(openssl passwd -6 root)
if [ -f ${WK}/etc/shadow ] && grep -q '^root:' ${WK}/etc/shadow; then
  sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
else
  echo "root:${HASH}:20000:0:99999:7:::" > ${WK}/etc/shadow
  chmod 640 ${WK}/etc/shadow
fi
echo "alpine" > ${WK}/etc/hostname
for i in mount-ro killprocs savecache; do
  ${CHROOT} ${WK} sh -c "${CPATH}; rc-update add ${i} shutdown" || \
    echo "WARNING: rc-update add ${i} shutdown failed"
done
# "networking" is deliberately NOT in this list.
for i in hwclock modules sysctl hostname bootmisc syslog; do
  ${CHROOT} ${WK} sh -c "${CPATH}; rc-update add ${i} boot" || \
    echo "WARNING: rc-update add ${i} boot failed"
done
for i in devfs dmesg mdev hwdrivers cgroups; do
  ${CHROOT} ${WK} sh -c "${CPATH}; rc-update add ${i} sysinit" || \
    echo "WARNING: rc-update add ${i} sysinit failed"
done
# qemu-guest-agent exits if its port node is not there yet: wait for it.
if [ -f ${WK}/etc/init.d/qemu-guest-agent ]; then
  if ! grep -q '^depend()' ${WK}/etc/init.d/qemu-guest-agent; then
    cat >> ${WK}/etc/init.d/qemu-guest-agent << 'EOF'

depend() {
	need dev
	after modules hwdrivers
}
EOF
  fi
fi
${CHROOT} ${WK} sh -c "${CPATH}; rc-update add qemu-guest-agent default" || \
  echo "WARNING: rc-update add qemu-guest-agent default failed"
${CHROOT} ${WK} sh -c "${CPATH}; rc-update add sshd default" || \
  echo "WARNING: sshd could not be added to the default runlevel"
# Confirm the agent really is in a runlevel: without it cloonix cannot reach
# the guest at all.
QGA_RL=$(cd ${WK}/etc/runlevels 2>/dev/null && ls */qemu-guest-agent 2>/dev/null | cut -d/ -f1 | head -1)
if [ -n "${QGA_RL}" ]; then
  echo "qemu-guest-agent: enabled in runlevel ${QGA_RL}"
  [ "${QGA_RL}" = "sysinit" ] && \
    echo "  WARNING: sysinit runs before the virtio port node exists"
else
  echo "WARNING: qemu-guest-agent is in no runlevel, cloonix will not reach the guest"
fi
if [ -f ${WK}/etc/inittab ]; then
  grep -q '^ttyS0:' ${WK}/etc/inittab || \
    echo 'ttyS0::respawn:/sbin/getty -L 115200 ttyS0 vt100' >> ${WK}/etc/inittab
fi
${CHROOT} ${WK} sh -c "${CPATH}; setup-keymap fr fr" 2>/dev/null || \
  echo "note: setup-keymap skipped"
# Point the image at the mirror address reachable from inside cloonix.
cat > ${WK}/etc/apk/repositories << EOF
${CNTREPO}/main
${CNTREPO}/community
EOF
# STEP 9: detach pseudo filesystems BEFORE mkfs, otherwise mkfs.ext4 -d would
# copy the host's /dev, /proc and /sys into the image.
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
mkdir -p ${WK}/dev ${WK}/proc ${WK}/sys ${WK}/tmp
rm -rf ${WK}/var/cache/apk/* 2>/dev/null || true
rm -f ${DISK}
truncate --size ${DISK_SIZE_G}G ${DISK}
${SGDISK} --clear \
  --new 1:${P1_START}:+1M --typecode=1:ef02 --change-name=1:'BIOS' \
  --new 2:${P2_START}:-0  --typecode=2:8300 --change-name=2:'Linux' ${DISK}
FS_SECTORS=$(( DISK_SIZE_G * 1024 * 1024 * 1024 / SECTOR - P2_START - 2048 ))
FS_BLOCKS=$(( FS_SECTORS * SECTOR / 4096 ))
${MKFS} -F -L cloonix -U ${FSUUID} -b 4096 \
        -O ^orphan_file,^metadata_csum_seed \
        -E offset=$(( P2_START * SECTOR )) \
        -d ${WK} ${DISK} ${FS_BLOCKS}
for d in dev proc sys; do
  mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d}
done
mkdir -p ${WK}/tmp/grubdir
mount -t tmpfs /tmp ${WK}/tmp/grubdir
cp -f ${GDIR}/*.img ${WK}/tmp/grubdir/
cp -f ${GDIR}/*.mod ${WK}/tmp/grubdir/ 2>/dev/null || true
: > ${WK}/tmp/disk.img
mount --bind ${DISK} ${WK}/tmp/disk.img
set +e
${CHROOT} ${WK} sh -c "${CPATH} ${BIOS_SETUP} \
    --directory=/tmp/grubdir \
    --device-map=/dev/null \
    --skip-fs-probe \
    /tmp/disk.img"
RC=$?
set -e
umount ${WK}/tmp/disk.img 2>/dev/null || true
rm -f ${WK}/tmp/disk.img
umount ${WK}/tmp/grubdir 2>/dev/null || true
rmdir ${WK}/tmp/grubdir 2>/dev/null || true
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
[ ${RC} -eq 0 ] || { echo "ERROR: grub-bios-setup failed (rc=${RC})"; exit 1; }
# Sanity: the MBR must carry the 0x55AA signature and the ef02 partition must
# not be empty.
SIG=$(dd if=${DISK} bs=1 skip=510 count=2 status=none | od -An -tx1 | tr -d ' \n')
[ "${SIG}" = "55aa" ] || \
  { echo "ERROR: boot signature missing (got ${SIG})"; exit 1; }
EMB=$(dd if=${DISK} bs=${SECTOR} skip=${P1_START} count=1 status=none | \
      od -An -tx1 | tr -d ' \n' | tr -d '0')
[ -n "${EMB}" ] || \
  { echo "ERROR: BIOS boot partition is empty, embedding did not happen"; exit 1; }
echo "grub-bios-setup: MBR signature ok, core embedded at sector ${P1_START}"
if [ -e ${QCOW2} ]; then mv -f ${QCOW2} ${QCOW2}.prev; fi
qemu-img convert -O qcow2 -S 4k ${DISK} ${QCOW2}
rm -f ${DISK}
[ -e ${QCOW2}.prev ] && echo "previous image kept as ${QCOW2}.prev" || true
echo "----------------------------------------------------------------"
echo "qcow2 ready: ${QCOW2}"
echo "rootfs UUID: ${FSUUID}"
echo "kernel     : ${version}"
echo "initramfs  : ${INITRD}"
echo "core.img   : ${CORE_SIZE} bytes"
if [ "${KEEP_TREE}" = "1" ]; then
  echo "build tree kept in ${ROOTDIR} (KEEP_TREE=1)"
else
  rm -rf ${ROOTDIR}
fi
echo "----------------------------------------------------------------"
