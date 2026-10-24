#!/bin/bash
# strongswan_trixie: ../cnt_kvm/cnt_trixie.sh plus strongswan, and nothing
# else.
. "$(dirname "$0")/demo_common.sh"
demo_begin strongswan_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="strongswan,strongswan-swanctl,strongswan-pki,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc conntrack"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/swanctl" ]; then
  echo "ERROR: /usr/sbin/swanctl is not in ${TRIXIE}, the mirror does not"
  echo "       carry strongswan-swanctl"
  exit 1
fi
demo_apt
# charon runs as root; /etc/swanctl and its state claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/swanctl/x509 /etc/swanctl/x509ca /etc/swanctl/private /etc/swanctl/conf.d"
for i in "/etc/swanctl" "/etc/strongswan.d" "/var/lib/strongswan" "/etc/ipsec.d" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  strongswan_trixie: strongswan in place"
demo_end
