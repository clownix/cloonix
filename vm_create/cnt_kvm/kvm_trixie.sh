#!/bin/bash
# Debian trixie qcow2 image, built WITHOUT root privileges.
DISTRO="trixie"
ROOTDIR="/var/lib/cloonix/bulk/raw_${DISTRO}_dir"
DISK="/var/lib/cloonix/bulk/${DISTRO}_disk.img"
QCOW2="/var/lib/cloonix/bulk/${DISTRO}.qcow2"
# Local mirror used during the build.
REPO="http://127.0.0.1/${DISTRO}"
# Mirror address written into the image, reachable from inside cloonix.
CNTREPO="http://10.117.0.1/${DISTRO}"
MMDEBSTRAP="/usr/bin/mmdebstrap"
DISK_SIZE_G=32
P1_START=2048          # sectors
P1_SECTORS=2048        # 1 MiB BIOS boot partition (ef02)
P2_START=4096          # sectors
SECTOR=512
CORE_MAX=$((P1_SECTORS * SECTOR))
# sbin is not on a normal user's PATH on Debian.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
SGDISK=/usr/sbin/sgdisk
[ -x ${SGDISK} ] || SGDISK=$(command -v sgdisk 2>/dev/null)
MKFS=/usr/sbin/mkfs.ext4
[ -x ${MKFS} ] || MKFS=$(command -v mkfs.ext4 2>/dev/null)
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in wget unshare openssl qemu-img truncate dd od flock; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  for t in "${CHROOT}" "${SGDISK}" "${MKFS}" "${MMDEBSTRAP}"; do
    [ -n "$t" ] && [ -x "$t" ] || { echo "ERROR: missing $t"; exit 1; }
  done
  ${MKFS} -V 2>&1 | grep -qE 'mke2fs 1\.(4[3-9]|[5-9][0-9])' || \
    echo "WARNING: mkfs.ext4 may be too old for -d (need e2fsprogs >= 1.43)"
  rm -f ${DISK} 2>/dev/null || true
  if [ -e ${ROOTDIR} ]; then
    if [ "${KEEP_TREE}" = "1" ]; then
      echo "KEEP_TREE=1: reusing the existing tree in ${ROOTDIR}"
    else
      echo "removing previous tree ${ROOTDIR}"
      # Built with --map-auto: files are chowned to system accounts that land
      # as high subuids the login user cannot unlink.
      unshare --map-root-user --user --map-auto -- rm -rf ${ROOTDIR} 2>/dev/null || \
        rm -rf ${ROOTDIR} 2>/dev/null || true
      [ -e ${ROOTDIR} ] && { echo "ERROR: could not remove ${ROOTDIR}"; exit 1; }
    fi
  fi
  if [ -e ${QCOW2} ]; then
    echo "note: ${QCOW2} exists and will be replaced when the build succeeds"
  fi
  wget -q --spider ${REPO} || \
    { echo "${REPO} not reachable"; exit 1; }
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
if [ "${KEEP_TREE}" = "1" ] && [ -x ${WK}/usr/bin/bash ]; then
echo "KEEP_TREE=1: skipping STEP 1 (tree already built)"
mkdir -p ${WK}/{dev,proc,sys,tmp}
else
LIST="linux-image-amd64,grub-common,grub2-common,grub-pc-bin,"
LIST+="openssh-server,openssh-client,vim,zstd,bash-completion,net-tools,"
LIST+="file,qemu-guest-agent,locales,iperf3,htop,strace,console-setup,kbd,"
LIST+="keyboard-configuration,sudo,tcpdump,lsof,pciutils,iproute2,"
LIST+="isc-dhcp-client,ifupdown,hostapd,wpasupplicant,iw,e2fsprogs"
${MMDEBSTRAP} --variant=important \
      --include="${LIST}" \
      --aptopt='Acquire::AllowInsecureRepositories "true"' \
      --aptopt='APT::Get::AllowUnauthenticated "true"' \
      ${DISTRO} ${WK} \
      "deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main"
if [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: mmdebstrap did not produce a usable rootfs in ${WK}"
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
# Sanity: the packages the rest of the script depends on must be present.
for f in vim ip tcpdump grub-mkimage; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing from the rootfs"
      echo "       grub-mkimage comes from grub-common"
      exit 1; }
done
# The i386-pc module directory must exist, otherwise grub-mkimage has nothing
# to build a core image from.
[ -d ${WK}/usr/lib/grub/i386-pc ] || \
  { echo "ERROR: /usr/lib/grub/i386-pc missing (install grub-pc-bin)"; exit 1; }
# /usr/sbin/grub-bios-setup is a symlink made by grub-pc's postinst, absent here: use the one in /usr/lib/grub/i386-pc.
if ${CHROOT} ${WK} sh -c "${CPATH} command -v grub-bios-setup >/dev/null 2>&1"; then
  BIOS_SETUP="grub-bios-setup"
elif [ -x ${WK}/usr/lib/grub/i386-pc/grub-bios-setup ]; then
  BIOS_SETUP="/usr/lib/grub/i386-pc/grub-bios-setup"
else
  echo "ERROR: grub-bios-setup not found, neither on PATH nor in"
  echo "       /usr/lib/grub/i386-pc/ (is grub-pc-bin installed?)"
  exit 1
fi
echo "using bios-setup: ${BIOS_SETUP}"
# Only our entry.
rm -f ${WK}/etc/apt/sources.list.d/*.list \
      ${WK}/etc/apt/sources.list.d/*.sources 2>/dev/null || true
cat > ${WK}/etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${CNTREPO} ${DISTRO} main
EOF
rm -rf ${WK}/var/lib/apt/lists/partial
rm -rf ${WK}/var/cache/apt/archives/partial
vmlinuz=$(cd ${WK}/boot && ls vmlinuz-* 2>/dev/null | tail -1)
[ -n "${vmlinuz}" ] || { echo "ERROR: no kernel in /boot"; exit 1; }
version=${vmlinuz#vmlinuz-}
echo "kernel version: ${version}"
cat > ${WK}/etc/initramfs-tools/modules << EOF
virtio_scsi
virtio_blk
virtio_pci
virtio_console
virtio_net
sd_mod
ext4
EOF
sed -i 's/^MODULES=.*/MODULES=most/' ${WK}/etc/initramfs-tools/initramfs.conf \
  2>/dev/null || echo "MODULES=most" >> ${WK}/etc/initramfs-tools/initramfs.conf
${CHROOT} ${WK} sh -c "${CPATH} update-initramfs -u -k ${version}" || \
${CHROOT} ${WK} sh -c "${CPATH} update-initramfs -c -k ${version}"
[ -s ${WK}/boot/initrd.img-${version} ] || \
  { echo "ERROR: no initramfs produced"; exit 1; }
# STEP 3: filesystem UUID, chosen up front so grub.cfg and fstab can reference
# it before the filesystem exists.
FSUUID=$(cat /proc/sys/kernel/random/uuid)
echo "rootfs UUID: ${FSUUID}"
KERN="rw noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
mkdir -p ${WK}/boot/grub
cat > ${WK}/boot/grub/grub.cfg << EOF
set timeout=1
set default=0
insmod part_gpt
insmod ext2
insmod search_fs_uuid
insmod linux
search --no-floppy --fs-uuid --set=root ${FSUUID}
menuentry 'Debian GNU/Linux ${DISTRO}' {
    linux /boot/vmlinuz-${version} root=UUID=${FSUUID} ${KERN}
    initrd /boot/initrd.img-${version}
}
EOF
printf "\nGRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "${KERN}" \
        >> ${WK}/etc/default/grub
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
sed -i 's/^# *fr_FR.UTF-8 UTF-8/fr_FR.UTF-8 UTF-8/' ${WK}/etc/locale.gen 2>/dev/null || true
grep -q '^fr_FR.UTF-8 UTF-8' ${WK}/etc/locale.gen 2>/dev/null || \
  echo 'fr_FR.UTF-8 UTF-8' >> ${WK}/etc/locale.gen
grep -q '^en_US.UTF-8 UTF-8' ${WK}/etc/locale.gen 2>/dev/null || \
  echo 'en_US.UTF-8 UTF-8' >> ${WK}/etc/locale.gen
cat > ${WK}/etc/default/locale << EOF
LANG=fr_FR.UTF-8
EOF
cat > ${WK}/etc/default/keyboard << EOF
XKBMODEL="pc105"
XKBLAYOUT="fr"
XKBVARIANT="latin9"
XKBOPTIONS=""
BACKSPACE="guess"
EOF
cat > ${WK}/etc/default/console-setup << EOF
ACTIVE_CONSOLES="/dev/tty[1-6]"
CHARMAP="UTF-8"
CODESET="Lat15"
FONTFACE="Fixed"
FONTSIZE="8x16"
EOF
# locale-gen only writes files, so it does work in the namespace.
${CHROOT} ${WK} sh -c "${CPATH} locale-gen" || \
  echo "WARNING: locale-gen failed, locales may be incomplete"
# STEP 7: fstab, users, and the services cloonix needs.
mkdir -p ${WK}/etc/modules-load.d
cat > ${WK}/etc/modules-load.d/virtio.conf << EOF
virtio_console
virtio_scsi
virtio_pci
virtio_net
EOF
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
UHASH=$(openssl passwd -6 user)
if ! grep -q '^user:' ${WK}/etc/passwd 2>/dev/null; then
  echo 'user:x:1000:1000:user:/home/user:/bin/bash' >> ${WK}/etc/passwd
  echo 'user:x:1000:' >> ${WK}/etc/group
  echo "user:${UHASH}:20000:0:99999:7:::" >> ${WK}/etc/shadow
  mkdir -p ${WK}/home/user
  chown 1000:1000 ${WK}/home/user 2>/dev/null || true
  chmod 755 ${WK}/home/user
fi
echo "source /etc/bash_completion" >> ${WK}/root/.bashrc
# cloonix talks to the guest over a virtserialport named
# org.qemu.guest_agent.0, so qemu-guest-agent MUST be running.
WANTS=${WK}/etc/systemd/system/multi-user.target.wants
mkdir -p ${WANTS}
for svc in qemu-guest-agent ssh systemd-networkd; do
  if [ -f ${WK}/lib/systemd/system/${svc}.service ]; then
    ln -sf /lib/systemd/system/${svc}.service ${WANTS}/${svc}.service
    echo "enabled: ${svc}.service"
  elif [ -f ${WK}/usr/lib/systemd/system/${svc}.service ]; then
    ln -sf /usr/lib/systemd/system/${svc}.service ${WANTS}/${svc}.service
    echo "enabled: ${svc}.service"
  else
    echo "WARNING: ${svc}.service not found, not enabled"
  fi
done
# Mask the *-wait-online units.
for u in NetworkManager-wait-online systemd-networkd-wait-online \
         systemd-networkd-wait-online@ wicked-wait-online; do
  for d in usr/lib lib; do
    if [ -f ${WK}/${d}/systemd/system/${u}.service ]; then
      ln -sf /dev/null ${WK}/etc/systemd/system/${u}.service
      echo "masked: ${u}.service"
    fi
  done
done
# Serial console: cloonix attaches -serial chardev:char0.
mkdir -p ${WK}/etc/systemd/system/getty.target.wants
for d in lib usr/lib; do
  if [ -f ${WK}/${d}/systemd/system/serial-getty@.service ]; then
    ln -sf /${d}/systemd/system/serial-getty@.service \
       ${WK}/etc/systemd/system/getty.target.wants/serial-getty@ttyS0.service
    break
  fi
done
DEFVIM=$(ls ${WK}/usr/share/vim/vim*/defaults.vim 2>/dev/null | head -n1)
if [ -n "${DEFVIM}" ] && [ -f "${DEFVIM}" ]; then
  sed -i s"/filetype plugin/\"filetype plugin/" "${DEFVIM}"
  sed -i s"/set mouse/\"set mouse/" "${DEFVIM}"
fi
# STEP 8: detach pseudo filesystems BEFORE mkfs, otherwise mkfs.ext4 -d would
# copy the host's /dev, /proc and /sys into the image.
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
mkdir -p ${WK}/dev ${WK}/proc ${WK}/sys ${WK}/tmp
rm -rf ${WK}/var/cache/apt/archives/*.deb 2>/dev/null || true
rm -f ${DISK}
truncate --size ${DISK_SIZE_G}G ${DISK}
${SGDISK} --clear \
  --new 1:${P1_START}:+1M --typecode=1:ef02 --change-name=1:'BIOS' \
  --new 2:${P2_START}:-0  --typecode=2:8300 --change-name=2:'Linux' ${DISK}
# Filesystem size in 4k blocks: the partition runs from P2_START to the end of
# the disk, less the 1 MiB the GPT backup header needs.
FS_SECTORS=$(( DISK_SIZE_G * 1024 * 1024 * 1024 / SECTOR - P2_START - 2048 ))
FS_BLOCKS=$(( FS_SECTORS * SECTOR / 4096 ))
${MKFS} -F -L cloonix -U ${FSUUID} -b 4096 \
        -O ^orphan_file,^metadata_csum_seed \
        -E offset=$(( P2_START * SECTOR )) \
        -d ${WK} ${DISK} ${FS_BLOCKS}
# STEP 10: the partition table is already written above; the GPT survives the
# mkfs because the filesystem starts at the partition offset.
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
echo "core.img   : ${CORE_SIZE} bytes"
if [ "${KEEP_TREE}" = "1" ]; then
  echo "build tree kept in ${ROOTDIR} (KEEP_TREE=1)"
else
  rm -rf ${ROOTDIR}
fi
echo "----------------------------------------------------------------"
