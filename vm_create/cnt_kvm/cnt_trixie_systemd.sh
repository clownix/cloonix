#!/bin/bash
# trixie_systemd - the trixie container image WITH systemd.
HERE=`pwd`
DISTRO="trixie"
TRIXIE="/var/lib/cloonix/bulk/trixie_systemd"
REPO="http://127.0.0.1/trixie"
CNTREPO="http://10.117.0.1/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
if [ -e $TRIXIE ] || [ -e $TRIXIE.tar.gz ]; then
  echo $TRIXIE or $TRIXIE.tar.gz already exists, please delete
  exit 1
fi
wget -q --spider ${REPO}
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${TRIXIE}" \
        -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
# adduser is not decoration.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,systemd,dbus,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant"

# The local mirror's key may be unknown: sources.list entry marked trusted=yes.
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} --variant=minbase \
               --include=${LIST} \
               --aptopt='Acquire::AllowInsecureRepositories \"true\"' \
               --aptopt='APT::Get::AllowUnauthenticated \"true\"' \
               ${DISTRO} ${TRIXIE} \
               \"deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main\""
if [ ! -d "${TRIXIE}" ] || [ ! -x "${TRIXIE}/usr/bin/bash" ]; then
  echo "ERROR: mmdebstrap did not produce a usable rootfs in ${TRIXIE}"
  exit 1
fi
cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CNTREPO} ${DISTRO} main
EOF"
fct_unshare_chroot "rm -rf /var/lib/apt/lists/partial"
fct_unshare_chroot "rm -rf /var/cache/apt/archives/partial"
fct_unshare_chroot "${cmd}"
rm -f ${TRIXIE}/usr/bin/sh
cd ${TRIXIE}/usr/bin
ln -s /usr/bin/bash sh
cd ${HERE}
for i in "dev" "proc" "sys"; do
  chmod 755 ${TRIXIE}/${i}
done
chmod 700 ${TRIXIE}/root
# rsyslog, locale, hosts and the accounts: written into the image here,
# not by cloonixsbininit at every boot.
. "$(dirname "$0")/cnt_common.sh"
cnt_common_config "${TRIXIE}" || exit 1
cnt_common_pack "${TRIXIE}" || exit 1
