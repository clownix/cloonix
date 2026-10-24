#!/bin/bash
# ntp_trixie: ../cnt_kvm/cnt_trixie.sh plus chrony.
. "$(dirname "$0")/demo_common.sh"
demo_begin ntp_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="chrony,python3"

EXTRA="netcat-openbsd psmisc ethtool"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/chronyd" ]; then
  echo "ERROR: /usr/sbin/chronyd is not in ${TRIXIE}, the mirror does not"
  echo "       carry chrony"
  exit 1
fi
demo_apt
# chrony state (account _chrony): claimed for uid 0, run as root.
fct_unshare_chroot "mkdir -p /etc/chrony /var/lib/chrony /var/log/chrony"
for i in "/etc/chrony" "/var/lib/chrony" "/var/log/chrony" "/run/chrony" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  ntp_trixie: chrony in place"
demo_end
