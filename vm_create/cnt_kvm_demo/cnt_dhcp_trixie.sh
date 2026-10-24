#!/bin/bash
# dhcp_trixie: ../cnt_kvm/cnt_trixie.sh with a dhcp server, a relay and two
# clients, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin dhcp_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="kea-dhcp4-server,kea-common,dnsmasq-base,"
LIST+="dhcpcd-base,busybox"

EXTRA="isc-dhcp-relay psmisc ethtool python3"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/kea-dhcp4" ]; then
  echo "ERROR: kea-dhcp4 is not in ${TRIXIE}, the mirror does not carry it"
  exit 1
fi
if [ ! -x "${TRIXIE}/usr/sbin/dnsmasq" ]; then
  echo "ERROR: dnsmasq is not in ${TRIXIE}, so there would be no relay"
  exit 1
fi
demo_apt

# /var/lib/kea, /var/log/kea (_kea) and the dnsmasq dirs claimed; both run as root.
for i in "/etc/kea" "/var/lib/kea" "/var/log/kea" "/var/lib/misc" \
         "/etc/dnsmasq.d" "/var/lib/dhcpcd" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
fct_unshare_chroot "mkdir -p /etc/kea /var/lib/kea /var/log/kea /run/kea"
fct_unshare_all "chmod 755 ${TRIXIE}/etc/kea"
fct_unshare_all "chmod 1777 ${TRIXIE}/var/lib/kea ${TRIXIE}/var/log/kea"
# Say what is in the image.
echo "  dhcp_trixie: kea, dnsmasq and dhcpcd in place"
demo_end
