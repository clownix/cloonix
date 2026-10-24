#!/bin/bash
# batman_trixie: ../cnt_kvm/cnt_trixie.sh plus batctl, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin batman_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="batctl,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc bridge-utils"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/batctl" ]; then
  echo "ERROR: /usr/sbin/batctl is not in ${TRIXIE}, the mirror does not"
  echo "       carry batctl"
  exit 1
fi
demo_apt
# Nothing to give back: batctl keeps no state, everything is in the kernel.
# Say what is in the image.
echo "  batman_trixie: batctl in place"
demo_end
