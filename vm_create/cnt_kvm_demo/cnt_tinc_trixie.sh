#!/bin/bash
# tinc_trixie: ../cnt_kvm/cnt_trixie.sh plus tinc, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin tinc_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="tinc,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/tincd" ]; then
  echo "ERROR: /usr/sbin/tincd is not in ${TRIXIE}, the mirror does not"
  echo "       carry tinc"
  exit 1
fi
demo_apt
# Each node's host files and keys are written into /etc/tinc by the
# demo, so that tree is claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/tinc"
for i in "/etc/tinc" "/var/run/tinc" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  tinc_trixie: tinc in place"
demo_end
