#!/bin/bash
# radius_trixie: ../cnt_kvm/cnt_trixie.sh plus freeradius and a supplicant.
. "$(dirname "$0")/demo_common.sh"
demo_begin radius_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="freeradius,freeradius-utils,python3"

EXTRA="netcat-openbsd psmisc ethtool openssl"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/freeradius" ]; then
  echo "ERROR: /usr/sbin/freeradius is not in ${TRIXIE}, the mirror does not"
  echo "       carry freeradius"
  exit 1
fi
demo_apt
# The freeradius tree (freerad, mode 750) claimed; freeradius runs as root here.
fct_unshare_chroot "mkdir -p /etc/freeradius /var/log/freeradius /var/lib/freeradius"
for i in "/etc/freeradius" "/var/log/freeradius" "/var/run/freeradius" "/var/lib/freeradius" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  radius_trixie: freeradius in place"
demo_end
