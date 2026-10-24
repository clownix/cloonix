#!/bin/bash
# bird_trixie: ../cnt_kvm/cnt_trixie.sh plus bird, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin bird_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="bird2,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc mtr-tiny"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/bird" ]; then
  echo "ERROR: /usr/sbin/bird is not in ${TRIXIE}, the mirror does not"
  echo "       carry bird2"
  exit 1
fi
demo_apt
# /etc/bird and /run/bird claimed; bird runs as root.
fct_unshare_chroot "mkdir -p /etc/bird /var/log/bird /run/bird"
for i in "/etc/bird" "/var/log/bird" "/run/bird" "/var/lib/bird" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  bird_trixie: bird in place"
demo_end
