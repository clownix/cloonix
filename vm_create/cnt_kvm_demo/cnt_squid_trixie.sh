#!/bin/bash
# squid_trixie: ../cnt_kvm/cnt_trixie.sh plus squid.
. "$(dirname "$0")/demo_common.sh"
demo_begin squid_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="squid,curl,python3"

EXTRA="netcat-openbsd psmisc apache2-utils socat"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/squid-gnutls" ]; then
  echo "ERROR: /usr/sbin/squid-gnutls is not in ${TRIXIE}, the mirror does"
  echo "       not carry squid"
  exit 1
fi
demo_apt
# squid spool and logs (account proxy): claimed for uid 0, run as root.
fct_unshare_chroot "mkdir -p /etc/squid /var/spool/squid /var/log/squid"
for i in "/etc/squid" "/var/spool/squid" "/var/log/squid" "/var/cache/squid" "/run/squid" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  squid_trixie: squid in place"
demo_end
