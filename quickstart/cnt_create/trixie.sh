#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
DISTRO="trixie"
TRIXIE0="/var/lib/cloonix/bulk/trixie"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/trixie"
CVMREPO="http://172.17.0.2/debian/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
#----------------------------------------------------------------------#
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
#----------------------------------------------------------------------#
if [ -e $TRIXIE0 ]; then
  echo $TRIXIE0 already exists, please delete
  exit 1
fi
#----------------------------------------------------------------------#
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#-----------------------------------------------------------------------#
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${TRIXIE0}" \
        -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
#----------------------------------------------------------------------#
LIST="iputils-ping,net-tools,procps,iproute2,systemd,dbus,strace,"
LIST+="vim,kmod,hostapd,wpasupplicant,iw,tcpdump"
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} --variant=minbase \
               --include=${LIST} \
               ${DISTRO} ${TRIXIE0} ${REPO}"
#----------------------------------------------------------------------#
cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CVMREPO} ${DISTRO} main
EOF"
#----------------------------------------------------------------------#
fct_unshare_chroot "rm -rf /var/lib/apt/lists/partial"
fct_unshare_chroot "rm -rf /var/cache/apt/archives/partial"
#----------------------------------------------------------------------#
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
rm -f ${TRIXIE0}/usr/bin/sh
cd ${TRIXIE0}/usr/bin
ln -s /usr/bin/bash sh
cd ${HERE} 
for i in "dev" "proc" "sys"; do
  chmod 755 ${TRIXIE0}/${i}
done
chmod 777 ${TRIXIE0}/root
#-----------------------------------------------------------------------#



