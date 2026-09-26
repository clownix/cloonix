#!/bin/bash
# Arch Linux qcow2 image, built WITHOUT root privileges.
DISTRO="arch"
ROOTDIR="/var/lib/cloonix/bulk/raw_arch_dir"
DISK="/var/lib/cloonix/bulk/arch_disk.img"
QCOW2="/var/lib/cloonix/bulk/arch.qcow2"
BSTRAP="/tmp/arch-bootstrap"
# Cached bootstrap tarball (120M); kept between runs.
TARBALL="/tmp/arch-bootstrap-$(id -u).tar.zst"
MIRROR="http://127.0.0.1/arch"
CNTMIRROR="http://10.117.0.1/arch"
BOOTSTRAP_URL="${MIRROR}/iso/latest/archlinux-bootstrap-x86_64.tar.zst"
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
  for t in wget unshare openssl qemu-img truncate dd od flock tar zstd; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  for t in "${CHROOT}" "${SGDISK}" "${MKFS}"; do
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
  wget -q --spider --timeout=20 --tries=2 \
      ${MIRROR}/core/os/x86_64/ 1>/dev/null 2>&1 || \
    { echo "${MIRROR}/core/os/x86_64 not reachable"; exit 1; }
  echo "REPO OK ${MIRROR}/core/os/x86_64"
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
BROOT="${BSTRAP}/root.x86_64"
# A previous run that aborted mid-way can leave mounts behind inside the tree
# or the bootstrap env.
for d in tmp/disk.img tmp/grubdir dev proc sys; do
  umount -l ${WK}/${d} 2>/dev/null || true
done
umount -l ${BROOT}/mnt/target 2>/dev/null || true
for d in dev proc sys; do umount -l ${BROOT}/${d} 2>/dev/null || true; done
rm -f ${WK}/tmp/disk.img 2>/dev/null || true
rmdir ${WK}/tmp/grubdir 2>/dev/null || true
# STEP 1: pacstrap the target tree from inside the bootstrap environment.
if [ "${KEEP_TREE}" = "1" ] && [ -x ${WK}/usr/bin/bash ]; then
echo "KEEP_TREE=1: skipping STEP 1 (tree already built)"
mkdir -p ${WK}/{dev,proc,sys,tmp}
else
mkdir -p ${WK}/{dev,proc,sys,tmp}
# Fetch and unpack the bootstrap environment (contains pacman/pacstrap).
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
# Point the bootstrap env at the mirror and init the pacman keyring.
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
# With SigLevel = Never the keyring is not consulted, so pacman-key is
# skipped: it needs a working gpg agent which is unreliable here.
mkdir -p ${BROOT}/mnt/target
mount --bind ${WK} ${BROOT}/mnt/target
for d in dev proc sys; do
  mkdir -p ${BROOT}/mnt/target/${d}
  mountpoint -q ${BROOT}/mnt/target/${d} || \
    mount --rbind /${d} ${BROOT}/mnt/target/${d} 2>/dev/null || \
    echo "WARNING: could not pre-mount ${d} in the target"
done
PKGS="base linux linux-firmware grub openssh sudo dhcpcd "
PKGS+="qemu-guest-agent which iproute2 net-tools tcpdump mkinitcpio e2fsprogs "
# iptables named explicitly: libxtables has two providers and pacman would ask.
PKGS+="iptables"
# Packages that are nice to have but must not fail the build.
PKGS_OPT="vim"
# Resolve the list and check the mirror serves it, before any of it is
# downloaded.
. "$(dirname "$0")/arch_common.sh"
arch_check_pkgs_served "${CHROOT}" "${BROOT}" /mnt/target "${PKGS}" || exit 1
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
    echo "If the errors above are 404s on individual packages, the mirror"
    echo "is skewed: Arch is rolling, and core.db/extra.db name exact"
    echo "versions. When the databases are newer than the package files,"
    echo "pacman asks for packages the mirror does not have yet."
    echo "Re-run get_arch.sh to resync; it uses rsync --delay-updates so"
    echo "the databases are only published once the packages are in place."
    exit 1
  fi
fi
# Optional packages, best effort.
if [ -n "${PKGS_OPT}" ]; then
  ${CHROOT} ${BROOT} sh -c "${CPATH}; pacman --root /mnt/target \
      --cachedir /var/cache/pacman/pkg \
      --noconfirm --needed -S ${PKGS_OPT}" || \
    echo "WARNING: optional packages (${PKGS_OPT}) not installed"
fi
for d in dev proc sys; do umount -l ${BROOT}/mnt/target/${d} 2>/dev/null || true; done
umount ${BROOT}/mnt/target 2>/dev/null || true
for d in dev proc sys; do umount -l ${BROOT}/${d} 2>/dev/null || true; done
if [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: pacstrap did not produce a usable rootfs in ${WK}"
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
# Sanity: the tools the rest of the script needs must be present.
for f in grub-mkimage mkinitcpio; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing from the rootfs"; exit 1; }
done
[ -d ${WK}/usr/lib/grub/i386-pc ] || \
  { echo "ERROR: /usr/lib/grub/i386-pc missing (grub package)"; exit 1; }
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
# Mirror for the finished image.
mkdir -p ${WK}/etc/pacman.d
echo "Server = ${CNTMIRROR}/\$repo/os/\$arch" > ${WK}/etc/pacman.d/mirrorlist
sed -i 's/^SigLevel.*/SigLevel = Never/' ${WK}/etc/pacman.conf 2>/dev/null || true
vmlinuz=$(cd ${WK}/boot && ls vmlinuz-* 2>/dev/null | tail -1)
[ -n "${vmlinuz}" ] || { echo "ERROR: no kernel in /boot"; exit 1; }
echo "kernel image: ${vmlinuz}"
# MODULES: virtio disk drivers named explicitly.
if [ -f ${WK}/etc/mkinitcpio.conf ]; then
  sed -i 's/^MODULES=.*/MODULES=(virtio virtio_pci virtio_blk virtio_scsi virtio_net virtio_console sd_mod ext4)/' \
      ${WK}/etc/mkinitcpio.conf
fi
${CHROOT} ${WK} sh -c "${CPATH}; mkinitcpio -P" || \
  echo "WARNING: mkinitcpio reported errors"
INITRD=$(cd ${WK}/boot && ls initramfs-*.img 2>/dev/null | grep -v fallback | tail -1)
[ -n "${INITRD}" ] || INITRD=$(cd ${WK}/boot && ls initramfs-*.img 2>/dev/null | tail -1)
[ -n "${INITRD}" ] || { echo "ERROR: no initramfs in /boot"; exit 1; }
echo "initramfs: ${INITRD}"
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
menuentry 'Arch Linux' {
    linux /boot/${vmlinuz} root=UUID=${FSUUID} ${KERN}
    initrd /boot/${INITRD}
}
EOF
mkdir -p ${WK}/etc/default
printf "GRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "${KERN}" > ${WK}/etc/default/grub
# STEP 5: GRUB core image.
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
echo "arch" > ${WK}/etc/hostname
grep -q '^en_US.UTF-8' ${WK}/etc/locale.gen 2>/dev/null || \
  echo "en_US.UTF-8 UTF-8" >> ${WK}/etc/locale.gen
grep -q '^fr_FR.UTF-8' ${WK}/etc/locale.gen 2>/dev/null || \
  echo "fr_FR.UTF-8 UTF-8" >> ${WK}/etc/locale.gen
${CHROOT} ${WK} sh -c "${CPATH}; locale-gen" || \
  echo "WARNING: locale-gen failed"
echo "LANG=en_US.UTF-8" > ${WK}/etc/locale.conf
echo "KEYMAP=fr" > ${WK}/etc/vconsole.conf
HASH=$(openssl passwd -6 root)
if [ -f ${WK}/etc/shadow ] && grep -q '^root:' ${WK}/etc/shadow; then
  sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
else
  echo "root:${HASH}:20000:0:99999:7:::" > ${WK}/etc/shadow
  chmod 600 ${WK}/etc/shadow
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
WANTS=${WK}/etc/systemd/system/multi-user.target.wants
mkdir -p ${WANTS}
for svc in qemu-guest-agent sshd dhcpcd; do
  if [ -f ${WK}/usr/lib/systemd/system/${svc}.service ]; then
    ln -sf /usr/lib/systemd/system/${svc}.service ${WANTS}/${svc}.service
    echo "enabled: ${svc}.service"
  else
    echo "WARNING: ${svc}.service not found, not enabled"
  fi
done
# Mask the *-wait-online units.
for u in systemd-networkd-wait-online NetworkManager-wait-online; do
  if [ -f ${WK}/usr/lib/systemd/system/${u}.service ]; then
    ln -sf /dev/null ${WK}/etc/systemd/system/${u}.service
    echo "masked: ${u}.service"
  fi
done
# Serial console: cloonix attaches -serial chardev:char0.
mkdir -p ${WK}/etc/systemd/system/getty.target.wants
if [ -f ${WK}/usr/lib/systemd/system/serial-getty@.service ]; then
  ln -sf /usr/lib/systemd/system/serial-getty@.service \
     ${WK}/etc/systemd/system/getty.target.wants/serial-getty@ttyS0.service
fi
# STEP 8: detach pseudo filesystems BEFORE mkfs, otherwise mkfs.ext4 -d would
# copy the host's /dev, /proc and /sys into the image.
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
mkdir -p ${WK}/dev ${WK}/proc ${WK}/sys ${WK}/tmp
rm -rf ${WK}/var/cache/pacman/pkg/* 2>/dev/null || true
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
echo "kernel     : ${vmlinuz}"
echo "initramfs  : ${INITRD}"
echo "core.img   : ${CORE_SIZE} bytes"
if [ "${KEEP_TREE}" = "1" ]; then
  echo "build tree kept in ${ROOTDIR} (KEEP_TREE=1)"
else
  rm -rf ${ROOTDIR}
  rm -rf ${BSTRAP}
  # ${TARBALL} is deliberately kept: it is 120M and only changes when Arch
  # publishes a new bootstrap image.
fi
echo "----------------------------------------------------------------"
