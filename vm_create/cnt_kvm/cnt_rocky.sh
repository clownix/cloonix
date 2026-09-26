#!/bin/bash
# Rocky Linux container rootfs (directory, no kernel/grub).
releasever=10
ROCKY0="/var/lib/cloonix/bulk/rocky"
WK="${ROCKY0}"
# Local mirror.
BASE="http://127.0.0.1/rocky/${releasever}"
CNTBASE="http://10.117.0.1/rocky/${releasever}"
ROCKY_BASEOS="${BASE}/BaseOS/x86_64/os"
ROCKY_APPSTREAM="${BASE}/AppStream/x86_64/os"
CNT_BASEOS="${CNTBASE}/BaseOS/x86_64/os"
CNT_APPSTREAM="${CNTBASE}/AppStream/x86_64/os"
PKGCACHE=/tmp/rocky-cnt-pkgs
# chroot lives in /usr/sbin, which is not on a normal user's PATH.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=$(command -v chroot 2>/dev/null)
[ -n "${CHROOT}" ] && [ -x "${CHROOT}" ] || { echo "ERROR: chroot not found"; exit 1; }
# NOTE: --map-auto needs /etc/subuid and /etc/subgid entries for you, e.g.
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in dnf rpm wget unshare openssl; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  [ -e ${ROCKY0} ] || [ -e ${ROCKY0}.tar.gz ] && { echo "ERROR ${ROCKY0} or ${ROCKY0}.tar.gz exists, please erase"; exit 1; }
  wget -q --spider ${ROCKY_BASEOS}/ || \
    { echo "${ROCKY_BASEOS} not reachable"; exit 1; }
  mkdir -p $(dirname ${ROCKY0}) 2>/dev/null || true
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
set -e
fct_mount_pseudo_fs(){
  for d in dev sys proc; do
    mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d}
  done
}
REPO_ARGS="--repofrompath=baseos,${ROCKY_BASEOS}"
REPO_ARGS+=" --repofrompath=appstream,${ROCKY_APPSTREAM}"
REPO_ARGS+=" --repo=baseos --repo=appstream"
DNF_COMMON="--releasever=${releasever} --nogpgcheck --setopt=sslverify=0"
DNF_COMMON+=" --setopt=tsflags=nocontexts --setopt=install_weak_deps=False"
. "$(dirname "$0")/rpm_common.sh"
REPO_LOCAL=("baseos|Rocky ${releasever} BaseOS|${ROCKY_BASEOS}"
            "appstream|Rocky ${releasever} AppStream|${ROCKY_APPSTREAM}")
DNF_CHROOT="${DNF_COMMON} $(rpm_dnf_repo_args baseos appstream)"
mkdir -p ${WK}/{dev,proc,sys}
fct_mount_pseudo_fs
rm -rf "${PKGCACHE}" && mkdir -p "${PKGCACHE}"
dnf ${DNF_COMMON} --installroot=${WK} ${REPO_ARGS} \
    --downloadonly --downloaddir="${PKGCACHE}" \
    -y install filesystem setup rpm dnf
# excludepath=/dev : mknod is not permitted in a user namespace, and /dev is
# rbind-mounted from the host for the chroot steps anyway.
RPM_BOOT=(rpm --root=${WK} --noscripts --nodeps --replacepkgs --excludepath=/dev)
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/setup-*.rpm
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/*.rpm
if [ ! -x ${WK}/usr/bin/rpm ] || [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: bootstrap failed"
  exit 1
fi
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
cp -f /etc/resolv.conf ${WK}/etc/resolv.conf
[ -s ${WK}/etc/passwd ] || echo 'root:x:0:0:root:/root:/bin/bash' > ${WK}/etc/passwd
[ -s ${WK}/etc/group ]  || echo 'root:x:0:' > ${WK}/etc/group
${CHROOT} ${WK} sh -c 'PATH=/usr/sbin:/usr/bin:/sbin:/bin; command -v ldconfig >/dev/null 2>&1 && ldconfig || /sbin/ldconfig' || \
  echo "WARNING: ldconfig failed in the new root"
mkdir -p ${WK}/tmp
cp -f "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/setup-*.rpm ${WK}/tmp/ || \
  echo "WARNING: the filesystem packages could not be copied into the new root"
${CHROOT} ${WK} rpm -Uvh --replacepkgs --nodeps --excludepath=/dev \
       /tmp/filesystem-*.rpm /tmp/setup-*.rpm || \
  echo "WARNING: the filesystem packages could not be installed again from inside the new root"
rm -f ${WK}/tmp/filesystem-*.rpm ${WK}/tmp/setup-*.rpm
set +e
list_pkt="iputils procps-ng net-tools iproute systemd dbus rsyslog "
list_pkt+="strace vim tcpdump shadow-utils sudo which"
${CHROOT} ${WK} dnf ${DNF_CHROOT} clean packages
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install ca-certificates
${CHROOT} ${WK} sh -c 'PATH=/usr/sbin:/usr/bin:/sbin:/bin; command -v update-ca-certificates >/dev/null 2>&1 && update-ca-certificates'
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
rpm_check_repos_local "${WK}" || exit 1
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install $list_pkt || \
  { echo "ERROR: package install failed"; exit 1; }
# Sanity check: the chroot steps above must actually have produced the tools
# we asked for, otherwise the rootfs is only the bootstrap set.
for f in vim ip tcpdump; do
  ${CHROOT} ${WK} sh -c "PATH=/usr/sbin:/usr/bin:/sbin:/bin command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, install did not run"; exit 1; }
done
# Scriptlets that need real root are re-run best-effort here.
umount -l ${WK}/dev 2>/dev/null || true
${CHROOT} ${WK} sh -c 'PATH=/usr/sbin:/usr/bin:/sbin:/bin; command -v systemd-sysusers >/dev/null 2>&1 && systemd-sysusers'
${CHROOT} ${WK} sh -c 'PATH=/usr/sbin:/usr/bin:/sbin:/bin; command -v systemd-tmpfiles >/dev/null 2>&1 && systemd-tmpfiles --create'
# passwd(1) needs PAM and real root, so write the hash directly.
HASH=$(openssl passwd -6 root)
if [ -f ${WK}/etc/shadow ] && grep -q '^root:' ${WK}/etc/shadow; then
  sed -i "s|^root:[^:]*:|root:${HASH}:|" ${WK}/etc/shadow
else
  echo "root:${HASH}:20000:0:99999:7:::" > ${WK}/etc/shadow
  chmod 000 ${WK}/etc/shadow
fi
# Point the container at the mirror address reachable from inside cloonix.
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
sed -i -e "s|${ROCKY_BASEOS}|${CNT_BASEOS}|" \
       -e "s|${ROCKY_APPSTREAM}|${CNT_APPSTREAM}|" \
       ${WK}/etc/yum.repos.d/cloonix-local.repo
rpm_check_repos_local "${WK}" || exit 1
# Unmount the pseudo filesystems first: rm/chmod on the host's mountpoints is EPERM.
for d in proc sys; do umount -l ${WK}/${d} 2>/dev/null || true; done
rm -rf ${WK:?}/dev/*
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
