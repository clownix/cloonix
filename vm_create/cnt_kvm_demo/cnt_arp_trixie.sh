#!/bin/bash
# arp_trixie: ../cnt_kvm/cnt_trixie.sh plus dsniff (arpspoof) and arping.
. "$(dirname "$0")/demo_common.sh"
demo_begin arp_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="dsniff,iputils-arping,curl,python3"

EXTRA="netcat-openbsd psmisc ethtool"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/arpspoof" ] && [ ! -x "${TRIXIE}/usr/bin/arpspoof" ]; then
  echo "ERROR: arpspoof is not in ${TRIXIE}, the mirror does not carry dsniff"
  exit 1
fi
demo_apt
# Say what is in the image.
echo "  arp_trixie: dsniff (arpspoof) and arping in place"
demo_end
