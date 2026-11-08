#!/bin/bash
# OpenWrt qcow2 image, built WITHOUT root privileges: the boot sectors, grub
# and kernel of the mirrored combined image, its root partition rebuilt from
# the mirrored rootfs tarball with qemu-ga. The kernel boots without initrd:
# virtio-scsi, sd, sr and iso9660 are built in, all cloonix needs.
ROOTDIR="/var/lib/cloonix/bulk/raw_openwrt_dir"
DISK="/var/lib/cloonix/bulk/openwrt_disk.img"
QCOW2="/var/lib/cloonix/bulk/openwrt.qcow2"
DISK_SIZE_G=32
SECTOR=512
# sbin is not on a normal user's PATH on Debian hosts.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
MKFS=/usr/sbin/mkfs.ext4
[ -x ${MKFS} ] || MKFS=$(command -v mkfs.ext4 2>/dev/null)
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
. "$(dirname "$0")/openwrt_common.sh"
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare openssl qemu-img truncate dd od flock tar gzip sha256sum; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  for t in "${CHROOT}" "${MKFS}"; do
    [ -n "$t" ] && [ -x "$t" ] || { echo "ERROR: missing $t"; exit 1; }
  done
  ${MKFS} -V 2>&1 | grep -qE 'mke2fs 1\.(4[3-9]|[5-9][0-9])' || \
    echo "WARNING: mkfs.ext4 may be too old for -d (need e2fsprogs >= 1.43)"
  wget -q --spider "${OPENWRT_TARGET}/sha256sums" || \
    { echo "ERROR: ${OPENWRT_TARGET}/sha256sums not reachable"
      echo "       run get_openwrt.sh to mirror openwrt,"
      echo "       then check_www_links.sh to publish it"
      exit 1; }
  rm -f ${DISK}
  if [ -e ${ROOTDIR} ] && [ "${KEEP_TREE}" = "1" ]; then
    echo "KEEP_TREE=1: reusing the existing tree in ${ROOTDIR}"
  elif [ -e ${ROOTDIR} ]; then
    echo "removing previous tree ${ROOTDIR}"
    # Built with --map-auto: apk chowns files to system accounts that land
    # as high subuids the login user cannot unlink.
    unshare --map-root-user --user --map-auto -- rm -rf ${ROOTDIR} || rm -rf ${ROOTDIR}
    [ -e ${ROOTDIR} ] && { echo "ERROR: could not remove ${ROOTDIR}"; exit 1; }
  fi
  [ -e ${QCOW2} ] && echo "note: ${QCOW2} exists and will be replaced when the build succeeds"
  mkdir -p "$(dirname ${ROOTDIR})"
  # These builders must NOT run concurrently.
  LOCKFILE=/var/lib/cloonix/bulk
  exec 9<${LOCKFILE}
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
set -eE
trap 'echo "ERROR: line ${LINENO}: ${BASH_COMMAND} failed"' ERR
WK="${ROOTDIR}"
if [ "${KEEP_TREE}" = "1" ] && [ -x ${WK}/usr/bin/apk ]; then
echo "KEEP_TREE=1: skipping the tree build (tree already built)"
else
openwrt_tree "${WK}"
openwrt_add "${WK}" qemu-ga tcpdump strace ip-full vim
for f in qemu-ga tcpdump strace vim; do
  ${CHROOT} ${WK} sh -c "${CPATH}; command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, the install did not run"; exit 1; }
done
# cloonix reaches the guest through qemu-ga, started by procd; it opens
# /dev/virtio-ports/org.qemu.guest_agent.0, a link the hotplug script makes.
${CHROOT} ${WK} sh -c "${CPATH}; /etc/init.d/qemu-ga enable"
ls ${WK}/etc/rc.d/S*qemu-ga >/dev/null || { echo "ERROR: qemu-ga is not started at boot"; exit 1; }
[ -x ${WK}/etc/hotplug.d/virtio-ports/00-virtio-ports ] || \
  { echo "ERROR: no virtio-ports hotplug script, qemu-ga would find no port"; exit 1; }
HASH=$(openssl passwd -6 root)
sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
grep -q "^root:${HASH}:" ${WK}/etc/shadow || { echo "ERROR: no root line in /etc/shadow"; exit 1; }
# Point the VM at the mirror address reachable from inside cloonix.
openwrt_feeds "${WK}" "${OPENWRT_CNT_REPO}"
openwrt_untree "${WK}"
fi
# MBR, grub and boot partition of the combined image, partition 2 its root.
NAME=$(openwrt_file generic-ext4-combined.img.gz)
IMGGZ=$(mktemp --suffix=.img.gz)
openwrt_fetch "${NAME}" "${IMGGZ}"
BASE=$(mktemp --suffix=.img)
# OpenWrt pads the gzip stream: gzip says trailing garbage ignored, exit 2.
RC=0
gzip -dc "${IMGGZ}" > "${BASE}" || RC=$?
rm -f "${IMGGZ}"
[ ${RC} -eq 0 ] || [ ${RC} -eq 2 ] || { rm -f "${BASE}"; echo "ERROR: gunzip of ${NAME} failed (rc=${RC})"; exit 1; }
SIG=$(dd if=${BASE} bs=1 skip=510 count=2 status=none | od -An -tx1 | tr -d ' \n')
P2_TYPE=$(od -An -tx1 -j 466 -N 1 ${BASE} | tr -d ' \n')
P2_START=$(od -An -tu4 -j 470 -N 4 ${BASE} | tr -d ' \n')
if [ "${SIG}" != "55aa" ] || [ "${P2_TYPE}" != "83" ] || [ "${P2_START}" -le 0 ]; then
  rm -f "${BASE}"
  echo "ERROR: ${NAME} is not an MBR disk with a linux partition 2 (${SIG} ${P2_TYPE} ${P2_START})"
  exit 1
fi
echo "${NAME}: partition 2 at sector ${P2_START}"
P2_SECTORS=$(( DISK_SIZE_G * 1024 * 1024 * 1024 / SECTOR - P2_START ))
rm -f ${DISK}
truncate --size ${DISK_SIZE_G}G ${DISK}
dd if=${BASE} of=${DISK} bs=${SECTOR} count=${P2_START} conv=notrunc status=none
rm -f "${BASE}"
# Partition 2 to the end of the disk: its sector count, little endian; the
# disk signature, so the root=PARTUUID of grub.cfg, stays.
printf "$(printf '\\%03o' $((P2_SECTORS & 255)) $(((P2_SECTORS >> 8) & 255)) \
                         $(((P2_SECTORS >> 16) & 255)) $(((P2_SECTORS >> 24) & 255)))" | \
  dd of=${DISK} bs=1 seek=474 conv=notrunc status=none
[ "$(od -An -tu4 -j 474 -N 4 ${DISK} | tr -d ' \n')" = "${P2_SECTORS}" ] || \
  { echo "ERROR: partition 2 could not be resized"; exit 1; }
${MKFS} -F -L rootfs -b 4096 \
        -O ^orphan_file,^metadata_csum_seed \
        -E offset=$(( P2_START * SECTOR )) \
        -d ${WK} ${DISK} $(( P2_SECTORS * SECTOR / 4096 ))
MAGIC=$(od -An -tx1 -j $(( P2_START * SECTOR + 1080 )) -N 2 ${DISK} | tr -d ' \n')
[ "${MAGIC}" = "53ef" ] || { echo "ERROR: no ext4 at partition 2 (magic ${MAGIC})"; exit 1; }
if [ -e ${QCOW2} ]; then mv -f ${QCOW2} ${QCOW2}.prev; fi
qemu-img convert -O qcow2 -S 4k ${DISK} ${QCOW2}
rm -f ${DISK}
[ -e ${QCOW2}.prev ] && echo "previous image kept as ${QCOW2}.prev"
echo "----------------------------------------------------------------"
echo "qcow2 ready: ${QCOW2}"
echo "from       : ${NAME}"
if [ "${KEEP_TREE}" = "1" ]; then
  echo "build tree kept in ${ROOTDIR} (KEEP_TREE=1)"
else
  rm -rf ${ROOTDIR}
fi
echo "----------------------------------------------------------------"
