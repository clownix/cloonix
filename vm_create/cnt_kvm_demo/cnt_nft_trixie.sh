#!/bin/bash
# nft_trixie: ../cnt_kvm/cnt_trixie.sh plus nftables and the conntrack tools.
. "$(dirname "$0")/demo_common.sh"
demo_begin nft_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="nftables,conntrack,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc ethtool ipset"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/nft" ]; then
  echo "ERROR: /usr/sbin/nft is not in ${TRIXIE}, the mirror does not"
  echo "       carry nftables"
  exit 1
fi
demo_apt
# Nothing to claim: the nft ruleset is in the kernel, per namespace.
for i in "/etc/nftables.conf" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  nft_trixie: nftables in place"
demo_end
