#!/bin/bash
# ebpf_trixie: ../cnt_kvm/cnt_trixie.sh plus the bpf toolchain.
. "$(dirname "$0")/demo_common.sh"
demo_begin ebpf_trixie
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="bpftool,clang,llvm,libbpf-dev,libelf-dev,zlib1g-dev,gcc,make,python3"

EXTRA="iperf3 netcat-openbsd psmisc ethtool linux-headers-amd64"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/bpftool" ]; then
  echo "ERROR: /usr/sbin/bpftool is not in ${TRIXIE}, the mirror does not"
  echo "       carry bpftool"
  exit 1
fi
demo_apt
# /root/bpf is where the demo puts its sources and objects, and
# /sys/fs/bpf is the mount point for bpffs. Both are claimed for uid 0.
fct_unshare_chroot "mkdir -p /sys/fs/bpf /root/bpf"
for i in "/sys/fs/bpf" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  ebpf_trixie: the bpf toolchain in place"
demo_end
