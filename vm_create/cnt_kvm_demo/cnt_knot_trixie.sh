#!/bin/bash
# knot_trixie: ../cnt_kvm/cnt_trixie.sh plus knot dns.
. "$(dirname "$0")/demo_common.sh"
demo_begin knot_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="knot,knot-dnsutils,python3"

EXTRA="ldnsutils bind9-dnsutils netcat-openbsd psmisc"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/knotd" ]; then
  echo "ERROR: /usr/sbin/knotd is not in ${TRIXIE}, the mirror does not"
  echo "       carry knot"
  exit 1
fi
demo_apt
# /etc/knot and /var/lib/knot (account knot): claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/knot /var/lib/knot/keys /var/log/knot"
for i in "/etc/knot" "/var/lib/knot" "/run/knot" "/var/log/knot" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  knot_trixie: knot in place"
demo_end
