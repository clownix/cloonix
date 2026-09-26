#!/bin/bash
# openSUSE Tumbleweed container rootfs (directory, no kernel/grub).
DISTRO="tumbleweed"
SUSE0="/var/lib/cloonix/bulk/tumbleweed"
WK="${SUSE0}"
releasever="tumbleweed"
# Local mirror.
SUSE_OSS="http://127.0.0.1/tumbleweed/repo/oss"
CNT_OSS="http://10.117.0.1/tumbleweed/repo/oss"
PKGCACHE=/tmp/suse-cnt-pkgs
# chroot lives in /usr/sbin, which is not on a normal user's PATH.
CHROOT=/usr/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=/sbin/chroot
[ -x ${CHROOT} ] || CHROOT=$(command -v chroot 2>/dev/null)
[ -n "${CHROOT}" ] && [ -x "${CHROOT}" ] || { echo "ERROR: chroot not found"; exit 1; }
CPATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
# NOTE: --map-auto needs /etc/subuid and /etc/subgid entries for you.
if [ -z "${CLOONIX_IN_USERNS}" ]; then
  for t in dnf rpm wget unshare openssl; do
    command -v $t >/dev/null 2>&1 || { echo "ERROR: $t required"; exit 1; }
  done
  [ -e ${SUSE0} ] || [ -e ${SUSE0}.tar.gz ] && { echo "ERROR ${SUSE0} or ${SUSE0}.tar.gz exists, please erase"; exit 1; }
  wget -q --spider ${SUSE_OSS}/ || \
    { echo "${SUSE_OSS} not reachable"; exit 1; }
  echo "REPO OK ${SUSE_OSS}"
  mkdir -p $(dirname ${SUSE0}) 2>/dev/null || true
  export CLOONIX_IN_USERNS=1
  exec unshare --map-root-user --user --map-auto --mount -- "$0" "$@"
fi
set -e
fct_mount_pseudo_fs(){
  for d in dev sys proc; do
    mountpoint -q ${WK}/${d} || mount --rbind /${d} ${WK}/${d}
  done
}
REPO_ARGS="--repofrompath=oss,${SUSE_OSS} --repo=oss"
DNF_COMMON="--releasever=${releasever} --nogpgcheck --setopt=sslverify=0"
DNF_COMMON+=" --setopt=install_weak_deps=False"
. "$(dirname "$0")/rpm_common.sh"
REPO_LOCAL=("oss|openSUSE Tumbleweed OSS|${SUSE_OSS}")
DNF_CHROOT="${DNF_COMMON} $(rpm_dnf_repo_args oss)"
mkdir -p ${WK}/{dev,proc,sys}
fct_mount_pseudo_fs
rm -rf "${PKGCACHE}" && mkdir -p "${PKGCACHE}"
dnf ${DNF_COMMON} --installroot=${WK} ${REPO_ARGS} \
    --downloadonly --downloaddir="${PKGCACHE}" \
    -y install filesystem aaa_base rpm dnf
# excludepath=/dev : mknod is not permitted in a user namespace.
RPM_BOOT=(rpm --root=${WK} --noscripts --nodeps --replacepkgs --excludepath=/dev)
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/aaa_base-*.rpm 2>/dev/null || \
  "${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/filesystem-*.rpm
"${RPM_BOOT[@]}" -Uvh "${PKGCACHE}"/*.rpm
if [ ! -x ${WK}/usr/bin/rpm ] || [ ! -x ${WK}/usr/bin/bash ]; then
  echo "ERROR: bootstrap unpack failed; no usable rootfs in ${WK}"
  exit 1
fi
# zypper reads /etc/zypp/repos.d, not /etc/yum.repos.d: pin there too.
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
zypp_local_repo_only "${WK}"
cp -f /etc/resolv.conf ${WK}/etc/resolv.conf
[ -s ${WK}/etc/passwd ] || echo 'root:x:0:0:root:/root:/bin/bash' > ${WK}/etc/passwd
[ -s ${WK}/etc/group ]  || echo 'root:x:0:'                      > ${WK}/etc/group
# ldconfig BEFORE any in-chroot rpm/dnf: on Tumbleweed zlib comes from zlib-ng
# and libz.so.1 only resolves once /etc/ld.so.cache is built.
${CHROOT} ${WK} sh -c "${CPATH}; ldconfig || /sbin/ldconfig || /usr/sbin/ldconfig" || \
  echo "WARNING: ldconfig failed in the new root"
mkdir -p ${WK}/tmp
cp -f "${PKGCACHE}"/filesystem-*.rpm "${PKGCACHE}"/aaa_base-*.rpm ${WK}/tmp/ || \
  echo "WARNING: the filesystem packages could not be copied into the new root"
${CHROOT} ${WK} rpm -Uvh --replacepkgs --nodeps --excludepath=/dev \
       /tmp/filesystem-*.rpm /tmp/aaa_base-*.rpm || \
  echo "WARNING: the filesystem packages could not be installed again from inside the new root"
rm -f ${WK}/tmp/filesystem-*.rpm ${WK}/tmp/aaa_base-*.rpm
${CHROOT} ${WK} sh -c "${CPATH}; ldconfig" || \
  echo "WARNING: ldconfig failed in the new root"
set +e
list_pkt="iputils procps net-tools iproute2 systemd dbus-1 rsyslog "
list_pkt+="strace vim kmod tcpdump shadow sudo which"
${CHROOT} ${WK} dnf ${DNF_CHROOT} clean packages
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install ca-certificates || \
  echo "WARNING: ca-certificates could not be installed"
${CHROOT} ${WK} sh -c "${CPATH}; if command -v update-ca-certificates >/dev/null 2>&1; then update-ca-certificates; fi" || \
  echo "WARNING: update-ca-certificates failed in the new root"
rpm_local_repo_only "${WK}" "${REPO_LOCAL[@]}"
zypp_local_repo_only "${WK}"
rpm_check_repos_local "${WK}" || exit 1
${CHROOT} ${WK} dnf ${DNF_CHROOT} -y install $list_pkt || \
  { echo "ERROR: package install failed"; exit 1; }
# Sanity: the chroot steps must have produced the tools we asked for.
for f in vim ip tcpdump; do
  ${CHROOT} ${WK} sh -c "${CPATH} command -v $f >/dev/null 2>&1" || \
    { echo "ERROR: $f missing, install did not run"; exit 1; }
done
# Scriptlets that need real root, best effort.
umount -l ${WK}/dev 2>/dev/null || true
${CHROOT} ${WK} sh -c "${CPATH}; if command -v systemd-sysusers >/dev/null 2>&1; then systemd-sysusers; fi" || \
  echo "WARNING: systemd-sysusers failed in the new root"
${CHROOT} ${WK} sh -c "${CPATH}; if command -v systemd-tmpfiles >/dev/null 2>&1; then systemd-tmpfiles --create; fi" || \
  echo "WARNING: systemd-tmpfiles failed in the new root"
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
zypp_local_repo_only "${WK}"
sed -i -e "s|${SUSE_OSS}|${CNT_OSS}|" ${WK}/etc/yum.repos.d/cloonix-local.repo
sed -i -e "s|${SUSE_OSS}|${CNT_OSS}|" ${WK}/etc/zypp/repos.d/cloonix-local.repo \
       2>/dev/null || true
rpm_check_repos_local "${WK}" || exit 1
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
