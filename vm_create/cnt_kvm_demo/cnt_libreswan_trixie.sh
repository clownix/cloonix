#!/bin/bash
# libreswan_trixie: ../cnt_kvm/cnt_trixie.sh plus libreswan, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin libreswan_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="libreswan,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc conntrack"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/ipsec" ]; then
  echo "ERROR: /usr/sbin/ipsec is not in ${TRIXIE}, the mirror does not"
  echo "       carry libreswan"
  exit 1
fi
demo_apt
# pluto runs as root, its NSS database in /var/lib/ipsec/nss: claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/ipsec.d /var/lib/ipsec/nss /run/pluto"
for i in "/etc/ipsec.d" "/var/lib/ipsec" "/run/pluto" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  libreswan_trixie: libreswan in place"
demo_end
