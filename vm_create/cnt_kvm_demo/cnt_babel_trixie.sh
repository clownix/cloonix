#!/bin/bash
# babel_trixie: ../cnt_kvm/cnt_trixie.sh plus babeld, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin babel_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="babeld,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc mtr-tiny"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/babeld" ]; then
  echo "ERROR: /usr/sbin/babeld is not in ${TRIXIE}, the mirror does not"
  echo "       carry babeld"
  exit 1
fi
demo_apt
# babeld keeps its state file under /var/lib/babeld and the demo writes
# its configuration at run time; both are claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/babeld /var/lib/babeld"
for i in "/etc/babeld" "/var/lib/babeld" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  babel_trixie: babeld in place"
demo_end
