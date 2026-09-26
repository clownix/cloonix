#!/bin/bash
HERE=`pwd`
DISTRO="resolute"
UBUNTU="/var/lib/cloonix/bulk/${DISTRO}"
REPO="http://127.0.0.1/${DISTRO}"
CNTREPO="http://10.117.0.1/${DISTRO}"
MMDEBSTRAP="/usr/bin/mmdebstrap"
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
if [ -e $UBUNTU ] || [ -e $UBUNTU.tar.gz ]; then
  echo $UBUNTU or $UBUNTU.tar.gz already exists, please delete
  exit 1
fi
wget -q --spider ${REPO}
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${UBUNTU}" \
        -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump"
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} --variant=minbase \
               --components=\"main universe\" \
               --include=${LIST} \
               --aptopt='Acquire::AllowInsecureRepositories \"true\"' \
               --aptopt='APT::Get::AllowUnauthenticated \"true\"' \
               ${DISTRO} ${UBUNTU} \
               \"deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main universe\""
if [ ! -d "${UBUNTU}" ] || [ ! -x "${UBUNTU}/usr/bin/bash" ]; then
  echo "ERROR: mmdebstrap did not produce a usable rootfs in ${UBUNTU}"
  exit 1
fi
# Only our entry.
fct_unshare_chroot "rm -f /etc/apt/sources.list.d/*.list /etc/apt/sources.list.d/*.sources 2>/dev/null || true"
cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CNTREPO} ${DISTRO} main universe
EOF"
fct_unshare_chroot "rm -rf /var/lib/apt/lists/partial"
fct_unshare_chroot "rm -rf /var/cache/apt/archives/partial"
fct_unshare_chroot "${cmd}"
rm -f ${UBUNTU}/usr/bin/sh
cd ${UBUNTU}/usr/bin
ln -s /usr/bin/bash sh
cd ${HERE}
for i in "dev" "proc" "sys"; do
  chmod 755 ${UBUNTU}/${i}
done
chmod 700 ${UBUNTU}/root
# chown -R 0:0 inside the --map-auto namespace: files chowned to system accounts
# by postinsts land as subuids the login user cannot unlink. cloonix sets owners at launch.
unshare --map-root-user --user --map-auto --mount -- \
        chown -R 0:0 "${UBUNTU}"
# rsyslog, locale, hosts and the accounts: written into the image here,
# not by cloonixsbininit at every boot.
. "$(dirname "$0")/cnt_common.sh"
cnt_common_config "${UBUNTU}" || exit 1
cnt_common_pack "${UBUNTU}" || exit 1
