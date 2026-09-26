#!/bin/bash
# Fedora qcow2 image, built WITHOUT root privileges.
releasever=44
ROOTDIR="/var/lib/cloonix/bulk/raw_fedora${releasever}_dir"
DISK="/var/lib/cloonix/bulk/fedora${releasever}_disk.img"
QCOW2="/var/lib/cloonix/bulk/fedora${releasever}.qcow2"
PKGCACHE="/tmp/fedora-kvm-pkgs"
# Local mirror.
MIRROR="http://127.0.0.1/fedora${releasever}"
CNT_MIRROR="http://10.117.0.1/fedora${releasever}"
FEDORA_MAIN="${MIRROR}/releases/${releasever}/Everything/x86_64/os"
FEDORA_UPDATES="${MIRROR}/updates/${releasever}/Everything/x86_64"
CNT_MAIN="${CNT_MIRROR}/releases/${releasever}/Everything/x86_64/os"
CNT_UPDATES="${CNT_MIRROR}/updates/${releasever}/Everything/x86_64"
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
  for t in dnf rpm wget unshare openssl qemu-img truncate dd od flock; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  for t in "${CHROOT}" "${SGDISK}" "${MKFS}"; do
    [ -n "$t" ] && [ -x "$t" ] || { echo "ERROR: missing $t"; exit 1; }
  done
  # mkfs.ext4 -d needs e2fsprogs 1.43+
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
  wget -q --spider ${FEDORA_MAIN}/ || \
    { echo "${FEDORA_MAIN} not reachable"; exit 1; }
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
REPO_ARGS="--repofrompath=fedora,${FEDORA_MAIN}"
REPO_ARGS+=" --repofrompath=updates,${FEDORA_UPDATES}"
REPO_ARGS+=" --repo=fedora --repo=updates"
DNF_COMMON="--releasever=${releasever} --nogpgcheck --setopt=sslverify=0"
DNF_COMMON+=" --setopt=tsflags=nocontexts --setopt=install_weak_deps=False"
# The chroot dnf is pinned to the local mirror by id, not by hoping the repo
# files are right.
. "$(dirname "$0")/rpm_common.sh"
REPO_LOCAL=("fedora|Fedora ${releasever} Everything|${FEDORA_MAIN}"
            "updates|Fedora ${releasever} updates|${FEDORA_UPDATES}")
DNF_CHROOT="${DNF_COMMON} $(rpm_dnf_repo_args fedora updates)"
if [ "${KEEP_TREE}" = "1" ] && [ -x ${WK}/usr/bin/bash ]; then
echo "KEEP_TREE=1: skipping STEP 1 and STEP 2 (tree already built)"
mkdir -p ${WK}/{dev,proc,sys,tmp}
for d in dev sys proc; do
  mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d}
done
else
# STEP 1: bootstrap a minimal tree, same proven sequence as cnt_rocky.sh
mkdir -p ${WK}/{dev,proc,sys}
for d in dev sys proc; do
  mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d}
done
rm -rf "${PKGCACHE}" && mkdir -p "${PKGCACHE}"
dnf ${DNF_COMMON} --installroot=${WK} ${REPO_ARGS} \
    --downloadonly --downloaddir="${PKGCACHE}" \
    -y install filesystem setup rpm dnf
# Fedora 42+ rpm 6 uses a sqlite rpmdb; the host rpm 4.x must be told so.
RPM_BOOT=(rpm --root=${WK} --noscripts --nodeps --replacepkgs --excludepath=/dev
          --define '_db_backend sqlite')
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/setup-*.rpm
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/*.rpm
if [ ! -x ${WK}/usr/bin/rpm ] || [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: bootstrap failed"; exit 1
fi
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
cp -f /etc/resolv.conf ${WK}/etc/resolv.conf
[ -s ${WK}/etc/passwd ] || echo 'root:x:0:0:root:/root:/bin/bash' > ${WK}/etc/passwd
[ -s ${WK}/etc/group ]  || echo 'root:x:0:' > ${WK}/etc/group
${CHROOT} ${WK} sh -c "${CPATH}; command -v ldconfig >/dev/null 2>&1 && ldconfig || /sbin/ldconfig" || \
  echo "WARNING: ldconfig failed in the new root"
mkdir -p ${WK}/tmp
cp -f "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/setup-*.rpm ${WK}/tmp/ || \
  echo "WARNING: the filesystem packages could not be copied into the new root"
${CHROOT} ${WK} rpm -Uvh --replacepkgs --nodeps --excludepath=/dev \
       /tmp/filesystem-*.rpm /tmp/setup-*.rpm || \
  echo "WARNING: the filesystem packages could not be installed again from inside the new root"
rm -f ${WK}/tmp/filesystem-*.rpm ${WK}/tmp/setup-*.rpm
# STEP 2: install the VM package set (kernel, grub modules, tools)
list_pkt="kernel grub2-pc grub2-tools grub2-pc-modules openssh-server "
list_pkt+="openssh-clients iputils NetworkManager sudo kbd vim net-tools "
list_pkt+="dracut qemu-guest-agent passwd e2fsprogs dhcp-client"
# Re-applied after every transaction: dnf reads its config once at start.
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
${CHROOT} ${WK} dnf ${DNF_CHROOT} clean packages
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install ca-certificates || \
  echo "WARNING: ca-certificates could not be installed"
${CHROOT} ${WK} sh -c "${CPATH}; if command -v update-ca-certificates >/dev/null 2>&1; then update-ca-certificates; fi" || \
  echo "WARNING: update-ca-certificates failed in the new root"
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
rpm_check_repos_local "${WK}" || exit 1
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install ${list_pkt}
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
fi
vmlinuz=$(cd ${WK}/boot && ls vmlinuz-* 2>/dev/null | tail -1)
[ -n "${vmlinuz}" ] || { echo "ERROR: no kernel in /boot"; exit 1; }
version=${vmlinuz#vmlinuz-}
echo "kernel version: ${version}"
DRV="virtio_scsi virtio_blk virtio_console virtio_net virtio_pci"
${CHROOT} ${WK} sh -c "${CPATH}; dracut --no-hostonly --add-drivers \"${DRV}\" \
       --force /boot/initramfs-${version}.img ${version}"
[ -s ${WK}/boot/initramfs-${version}.img ] || { echo "ERROR: dracut produced no initramfs"; exit 1; }
FSUUID=$(cat /proc/sys/kernel/random/uuid)
echo "rootfs UUID: ${FSUUID}"
KERN="rw noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
# selinux=0 on the kernel line too: built with tsflags=nocontexts, no file has a label.
KERN+=" selinux=0"
mkdir -p ${WK}/boot/grub2
cat > ${WK}/boot/grub2/grub.cfg << EOF
set timeout=1
set default=0
insmod part_gpt
insmod ext2
insmod search_fs_uuid
insmod linux
search --no-floppy --fs-uuid --set=root ${FSUUID}
menuentry 'Fedora ${releasever}' {
    linux /boot/vmlinuz-${version} root=UUID=${FSUUID} ${KERN}
    initrd /boot/initramfs-${version}.img
}
EOF
printf "GRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "${KERN}" > ${WK}/etc/default/grub
GDIR=${WK}/boot/grub2/i386-pc
mkdir -p ${GDIR}
# grub2-bios-setup needs boot.img and diskboot.img alongside core.img.
cp -f ${WK}/usr/lib/grub/i386-pc/*.img ${GDIR}/ 2>/dev/null || true
cp -f ${WK}/usr/lib/grub/i386-pc/*.mod ${GDIR}/ 2>/dev/null || true
${CHROOT} ${WK} sh -c "${CPATH}; grub2-mkimage -O i386-pc \
    -p '(hd0,gpt2)/boot/grub2' -o /boot/grub2/i386-pc/core.img \
    biosdisk part_gpt ext2 search search_fs_uuid normal linux echo \
    configfile boot minicmd"
CORE=${GDIR}/core.img
[ -s ${CORE} ] || { echo "ERROR: grub2-mkimage produced nothing"; exit 1; }
CORE_SIZE=$(stat -c %s ${CORE})
echo "core.img: ${CORE_SIZE} bytes (limit ${CORE_MAX})"
[ ${CORE_SIZE} -le ${CORE_MAX} ] || \
  { echo "ERROR: core.img exceeds the BIOS boot partition"; exit 1; }
for f in boot.img diskboot.img; do
  [ -s ${GDIR}/${f} ] || { echo "ERROR: ${GDIR}/${f} missing"; exit 1; }
done
# STEP 7: fstab and root password Force virtio_console to load at boot.
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
  chmod 000 ${WK}/etc/shadow
fi
${CHROOT} ${WK} sh -c "${CPATH}; if command -v systemd-sysusers >/dev/null 2>&1; then systemd-sysusers; fi" || \
  echo "WARNING: systemd-sysusers failed in the new root"
# SELinux disabled: enforcing, qemu-ga cannot create and launch a script as root.
mkdir -p ${WK}/etc/sysconfig
cat > ${WK}/etc/sysconfig/selinux << EOF
SELINUX=disabled
SELINUXTYPE=targeted
EOF
cp -f ${WK}/etc/sysconfig/selinux ${WK}/etc/selinux/config 2>/dev/null || true
# qemu-ga blocks the file and exec RPCs by default.
cat > ${WK}/etc/sysconfig/qemu-ga << 'EOF'
# cloonix pushes its agent script into the guest and runs it, which needs
# the file and exec RPCs that qemu-ga filters out by default.
FILTER_RPC_ARGS="guest-file-open,guest-file-close,guest-file-read,guest-file-write,guest-exec-status,guest-exec"
EOF
# Enable the services cloonix needs.
QGA_WANTS=$(ls -d ${WK}/etc/systemd/system/dev-virtio*org.qemu.guest_agent.0.device.wants 2>/dev/null | head -1)
if [ -n "${QGA_WANTS}" ] && [ -e "${QGA_WANTS}/qemu-guest-agent.service" ]; then
  echo "qemu-guest-agent: device-activated wiring present"
elif [ -f ${WK}/usr/lib/systemd/system/qemu-guest-agent.service ]; then
  echo "qemu-guest-agent: device wiring absent, creating it"
  QGA_DIR="${WK}/etc/systemd/system/dev-virtio\\x2dports-org.qemu.guest_agent.0.device.wants"
  mkdir -p "${QGA_DIR}"
  ln -sf /usr/lib/systemd/system/qemu-guest-agent.service \
         "${QGA_DIR}/qemu-guest-agent.service"
else
  echo "WARNING: qemu-guest-agent.service not found, cloonix will not reach the guest"
fi
WANTS=${WK}/etc/systemd/system/multi-user.target.wants
mkdir -p ${WANTS}
for svc in sshd NetworkManager; do
  if [ -f ${WK}/usr/lib/systemd/system/${svc}.service ]; then
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
mkdir -p ${WK}/etc/systemd/system/getty.target.wants
if [ -f ${WK}/usr/lib/systemd/system/serial-getty@.service ]; then
  ln -sf /usr/lib/systemd/system/serial-getty@.service \
         ${WK}/etc/systemd/system/getty.target.wants/serial-getty@ttyS0.service
fi
# Point the VM at the mirror address reachable from inside cloonix.
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
sed -i -e "s|${FEDORA_MAIN}|${CNT_MAIN}|" \
       -e "s|${FEDORA_UPDATES}|${CNT_UPDATES}|" \
       ${WK}/etc/yum.repos.d/cloonix-local.repo
rpm_check_repos_local "${WK}" || exit 1
# STEP 8: detach pseudo filesystems BEFORE mkfs, otherwise mkfs.ext4 -d would
# try to copy the host's /dev, /proc and /sys into the image.
for d in dev proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/* ${WK}/proc/* ${WK}/sys/* 2>/dev/null || true
# keep the mountpoints themselves so step 10b can remount over them
mkdir -p ${WK}/dev ${WK}/proc ${WK}/sys ${WK}/tmp
rm -rf ${WK}/var/cache/dnf/* 2>/dev/null || true
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
mkdir -p ${WK}/tmp
: > ${WK}/tmp/disk.img
mount --bind ${DISK} ${WK}/tmp/disk.img
set +e
${CHROOT} ${WK} sh -c "${CPATH}; grub2-bios-setup \
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
[ ${RC} -eq 0 ] || { echo "ERROR: grub2-bios-setup failed (rc=${RC})"; exit 1; }
# Sanity: the MBR must now carry executable code and the 0x55AA signature, and
# the ef02 partition must not be empty.
SIG=$(dd if=${DISK} bs=1 skip=510 count=2 status=none | od -An -tx1 | tr -d ' \n')
[ "${SIG}" = "55aa" ] || \
  { echo "ERROR: boot signature missing after grub2-bios-setup (got ${SIG})"; exit 1; }
EMB=$(dd if=${DISK} bs=${SECTOR} skip=${P1_START} count=1 status=none | \
      od -An -tx1 | tr -d ' \n' | tr -d '0')
[ -n "${EMB}" ] || \
  { echo "ERROR: BIOS boot partition is empty, embedding did not happen"; exit 1; }
echo "grub2-bios-setup: MBR signature ok, core embedded at sector ${P1_START}"
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
