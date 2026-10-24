#!/bin/bash
# dns_trixie: ../cnt_kvm/cnt_trixie.sh with bind9, frr and the dns query tools
# added, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin dns_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="frr,frr-pythontools,"
LIST+="bind9,bind9-utils,bind9-dnsutils"

EXTRA="ldnsutils knot-dnsutils dnstracer whois netcat-openbsd python3"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/named" ]; then
  echo "ERROR: named is not in ${TRIXIE}, the mirror does not carry bind9"
  exit 1
fi
if [ ! -x "${TRIXIE}/usr/lib/frr/frrinit.sh" ]; then
  echo "ERROR: frrinit.sh is not in ${TRIXIE}, the mirror does not carry frr"
  exit 1
fi
demo_apt

# /etc/frr (frr), /etc/bind (group bind), /var/cache/bind (bind) claimed; named
# runs as root. Runtime dirs left world writable (lab image).
for i in "/etc/frr" "/var/log/frr" "/var/lib/frr" "/var/tmp/frr" \
         "/etc/bind" "/var/cache/bind" "/var/lib/bind" "/var/log/named" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
fct_unshare_all "chmod 755 ${TRIXIE}/etc/frr"
fct_unshare_all "chmod 755 ${TRIXIE}/etc/bind"
for i in "/var/log/frr" "/var/lib/frr" "/var/tmp/frr" \
         "/var/cache/bind" "/var/lib/bind" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chmod 1777 ${TRIXIE}${i}"
  fi
done
# Say what is in the image. A demo that asks for drill and finds it absent
# is a confusing half hour; finding it here is ten seconds.
echo "  dns_trixie: bind9 and frr in place"
demo_end
