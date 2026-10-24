#!/bin/bash
# flow_trixie: ../cnt_kvm/cnt_trixie.sh plus the traffic generators: iperf3
# for tcp and udp at any rate, trafgen (netsniff-ng) for raw frames at the
# packet rate of the machine, hping3 and socat for the odd shapes.
. "$(dirname "$0")/demo_common.sh"
demo_begin flow_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="iperf3,netsniff-ng,mtr-tiny,socat,ethtool,curl,python3"

EXTRA="netcat-openbsd psmisc nload iputils-tracepath hping3 nftables"
demo_strap
# netsniff-ng puts its tools in /usr/sbin, iperf3 is in /usr/bin.
for tool in usr/bin/iperf3 usr/sbin/trafgen; do
  if [ ! -x "${TRIXIE}/${tool}" ]; then
    echo "ERROR: /${tool} is not in ${TRIXIE}, the mirror does not"
    echo "       carry it (iperf3, netsniff-ng)"
    exit 1
  fi
done
demo_apt
# Nothing to claim: no state, no dropped privileges.
# Say what is in the image.
echo "  flow_trixie: iperf3 and trafgen in place"
demo_end
