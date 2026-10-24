#!/bin/bash
# vrrp_trixie: ../cnt_kvm/cnt_trixie.sh with keepalived added, and nothing
# else.
. "$(dirname "$0")/demo_common.sh"
demo_begin vrrp_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="keepalived,iputils-arping"

EXTRA="ipvsadm psmisc conntrack ethtool python3"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/keepalived" ]; then
  echo "ERROR: keepalived is not in ${TRIXIE}, the mirror does not carry it"
  exit 1
fi
demo_apt

# keepalived already keeps /etc/keepalived as root; claimed anyway.
for i in "/etc/keepalived" "/var/log/keepalived" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
fct_unshare_chroot "mkdir -p /etc/keepalived /run/keepalived"
fct_unshare_all "chmod 755 ${TRIXIE}/etc/keepalived"
# Say what is in the image. A demo that asks for drill and finds it absent
# is a confusing half hour; finding it here is ten seconds.
echo "  vrrp_trixie: keepalived in place"
demo_end
