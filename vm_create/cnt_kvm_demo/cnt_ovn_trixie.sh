#!/bin/bash
# ovn_trixie: ../cnt_kvm/cnt_trixie.sh plus open vswitch and ovn.
. "$(dirname "$0")/demo_common.sh"
demo_begin ovn_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="openvswitch-switch,openvswitch-common,ovn-common,ovn-central,ovn-host,python3"

EXTRA="iperf3 netcat-openbsd psmisc ethtool conntrack"
demo_strap
if [ ! -x "${TRIXIE}/usr/bin/ovn-nbctl" ]; then
  echo "ERROR: /usr/bin/ovn-nbctl is not in ${TRIXIE}, the mirror does not"
  echo "       carry ovn-common"
  exit 1
fi
demo_apt
# openvswitch trees: claimed for uid 0 (ovsdb database, ovn-controller socket).
fct_unshare_chroot "mkdir -p /etc/openvswitch /var/lib/openvswitch /var/log/openvswitch /etc/ovn /var/lib/ovn /var/log/ovn"
for i in "/etc/openvswitch" "/var/lib/openvswitch" "/var/log/openvswitch" "/var/run/openvswitch" "/etc/ovn" "/var/lib/ovn" "/var/log/ovn" "/var/run/ovn" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  ovn_trixie: ovs and ovn in place"
demo_end
