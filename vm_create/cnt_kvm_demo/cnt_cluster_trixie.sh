#!/bin/bash
# cluster_trixie: ../cnt_kvm/cnt_trixie.sh plus corosync and pacemaker.
. "$(dirname "$0")/demo_common.sh"
demo_begin cluster_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="corosync,pacemaker,pacemaker-cli-utils,crmsh,python3"

EXTRA="netcat-openbsd psmisc ethtool fence-agents"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/pacemakerd" ]; then
  echo "ERROR: /usr/sbin/pacemakerd is not in ${TRIXIE}, the mirror does not"
  echo "       carry pacemaker"
  exit 1
fi
demo_apt
# hacluster owns both state trees: claimed for uid 0, daemons run as root.
fct_unshare_chroot "mkdir -p /etc/corosync /var/lib/corosync /var/log/corosync /var/lib/pacemaker/cib /var/log/pacemaker"
for i in "/etc/corosync" "/var/lib/corosync" "/var/log/corosync" "/etc/pacemaker" "/var/lib/pacemaker" "/var/log/pacemaker" "/run/corosync" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  cluster_trixie: corosync and pacemaker in place"
demo_end
