#!/bin/bash
# unbound_trixie: ../cnt_kvm/cnt_trixie.sh plus unbound and the dnssec tools.
. "$(dirname "$0")/demo_common.sh"
demo_begin unbound_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="unbound,unbound-anchor,bind9-dnsutils,ldnsutils,python3"

EXTRA="knot-dnsutils netcat-openbsd psmisc dnstracer"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/unbound" ]; then
  echo "ERROR: /usr/sbin/unbound is not in ${TRIXIE}, the mirror does not"
  echo "       carry unbound"
  exit 1
fi
demo_apt
# /etc/unbound and /var/lib/unbound claimed; unbound runs as root (chroot and drop off in the demo config).
fct_unshare_chroot "mkdir -p /etc/unbound/unbound.conf.d /var/lib/unbound /var/log/unbound"
for i in "/etc/unbound" "/var/lib/unbound" "/var/log/unbound" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  unbound_trixie: unbound in place"
demo_end
