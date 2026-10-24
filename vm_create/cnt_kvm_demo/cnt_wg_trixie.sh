#!/bin/bash
# wg_trixie: ../cnt_kvm/cnt_trixie.sh with wireguard and a couple of ways to
# make traffic, and nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin wg_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="wireguard-tools,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc"
demo_strap
if [ ! -x "${TRIXIE}/usr/bin/wg" ]; then
  echo "ERROR: wg is not in ${TRIXIE}, the mirror does not carry"
  echo "       wireguard-tools"
  exit 1
fi
demo_apt

# Nothing to give back: wireguard lives in the kernel, keys go under /etc which is root already.
fct_unshare_chroot "mkdir -p /etc/wireguard"
fct_unshare_all "chmod 700 ${TRIXIE}/etc/wireguard"
# Say what is in the image.
echo "  wg_trixie: wireguard-tools in place"
demo_end
