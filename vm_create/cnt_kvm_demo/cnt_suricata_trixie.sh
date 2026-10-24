#!/bin/bash
# suricata_trixie: ../cnt_kvm/cnt_trixie.sh plus suricata.
. "$(dirname "$0")/demo_common.sh"
demo_begin suricata_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="suricata,curl,python3"

EXTRA="netcat-openbsd psmisc ethtool jq"
demo_strap
if [ ! -x "${TRIXIE}/usr/bin/suricata" ]; then
  echo "ERROR: /usr/bin/suricata is not in ${TRIXIE}, the mirror does not"
  echo "       carry suricata"
  exit 1
fi
demo_apt
# suricata logs and rules: claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/suricata/rules /var/lib/suricata/rules /var/log/suricata"
for i in "/etc/suricata" "/var/lib/suricata" "/var/log/suricata" "/run/suricata" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  suricata_trixie: suricata in place"
demo_end
