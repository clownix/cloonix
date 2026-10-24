#!/bin/bash
# openvpn_trixie: ../cnt_kvm/cnt_trixie.sh plus openvpn, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin openvpn_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="openvpn,easy-rsa,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/openvpn" ]; then
  echo "ERROR: /usr/sbin/openvpn is not in ${TRIXIE}, the mirror does not"
  echo "       carry openvpn"
  exit 1
fi
demo_apt
# The demo writes the whole PKI and both configurations into
# /etc/openvpn at run time, so that tree is claimed for uid 0.
fct_unshare_chroot "mkdir -p /etc/openvpn/pki /var/log/openvpn"
for i in "/etc/openvpn" "/var/log/openvpn" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  openvpn_trixie: openvpn in place"
demo_end
