#!/bin/bash
# perf_trixie: ../cnt_kvm/cnt_trixie.sh plus the measurement tools.
. "$(dirname "$0")/demo_common.sh"
demo_begin perf_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="iperf3,mtr-tiny,socat,ethtool,curl,python3"

EXTRA="netcat-openbsd psmisc nload iputils-tracepath hping3"
demo_strap
if [ ! -x "${TRIXIE}/usr/bin/iperf3" ]; then
  echo "ERROR: /usr/bin/iperf3 is not in ${TRIXIE}, the mirror does not"
  echo "       carry iperf3"
  exit 1
fi
demo_apt
# Nothing to claim: no state, no dropped privileges.
# Say what is in the image.
echo "  perf_trixie: the measurement tools in place"
demo_end
