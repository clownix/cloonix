#!/bin/bash
set -x
HERE=`pwd`
DISTRO="trixie"
RAW="seedbuilder.qcow2"
QCOW2="builder.qcow2"
NET="mito"
NAME="dev"
BULK="/var/lib/cloonix/bulk"
REPO="http://10.117.0.1/${DISTRO}"
if [ ! -e ${BULK}/${RAW} ]; then
  echo "ERROR ${BULK}/${RAW} not found: ./seedbuilder.sh makes it"
  exit 1
fi
is_started=$(cloonix cli $NET dump pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix start $NET:\n"
  cloonix start $NET
else
  cloonix cli $NET del all
fi
echo cp -f ${BULK}/${RAW} ${BULK}/${QCOW2}
cp -f ${BULK}/${RAW} ${BULK}/${QCOW2} || exit 1
sync
PARAMS="ram=8000 cpu=4 eth=1"
cloonix cli ${NET} add kvm ${NAME} ${PARAMS} ${QCOW2} --persistent
sleep 3
cloonix cli ${NET} plug ${NAME} eth0 lan1
set +e
echo WAIT
while ! cloonix cli ${NET} ssh ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 5 sec
  sleep 5
done
echo GO
cloonix cli ${NET} add nat nat
sleep 5
cloonix cli ${NET} plug nat lan1
cloonix cli ${NET} ssh ${NAME} "cat > /etc/apt/sources.list << EOF
deb ${REPO} ${DISTRO} main contrib non-free non-free-firmware
EOF"
cloonix cli ${NET} ssh ${NAME} "dhcpcd eth0"
cloonix cli ${NET} ssh ${NAME} "DEBIAN_FRONTEND=noninteractive \
                        DEBCONF_NONINTERACTIVE_SEEN=true \
                        apt-get -o Dpkg::Options::=--force-confdef \
                        -o Acquire::Check-Valid-Until=false \
                        --allow-unauthenticated --assume-yes update"
cloonix cli ${NET} ssh ${NAME} "DEBIAN_FRONTEND=noninteractive \
                        DEBCONF_NONINTERACTIVE_SEEN=true \
                        apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
  build-essential g++ gcc-12-multilib make cmake meson ninja-build \
  autoconf automake libtool pkg-config patch patchelf git flex bison \
  gperf libglib2.0-dev libpixman-1-dev zlib1g-dev libssl-dev \
  libpcre2-dev libpcap-dev libcap-dev libseccomp-dev libsystemd-dev \
  libbpf-dev libyajl-dev libfdt-dev libaio-dev liburing-dev \
  libnl-3-dev libnl-cli-3-dev libmnl-dev libelf-dev libreadline-dev \
  libjson-c-dev libyang-dev libprotobuf-c-dev protobuf-c-compiler \
  texinfo libgtk-3-dev libnuma-dev libunwind-dev xorriso x11vnc \
  x11-xserver-utils x11-xkb-utils sudo xauth locales rsyslog \
  util-linux tar unzip zip bzip2 xz-utils curl wget rsync lsof strace gdb \
  python3 python3-venv python3-dev python3-pip python3-wheel \
  python3-setuptools python3-wheel python3-pyparsing python3-six \
  arping iproute2 tcpdump iputils-tracepath iperf3 daemonize gcc-14-plugin-dev \
  gcc-mingw-w64-x86-64 \
  clang lld llvm mmdebstrap"
# The two cross toolchains: gcc-mingw-w64-x86-64 (windows agent), clang lld llvm
# (freebsd agent: only its linker and strip read a freebsd elf). build_cloonfs
# checks for them and stops rather than build an agent iso without its agent.
# The images of the specialised toys, built into the bulk of the builder.
TMPD=$(mktemp -d)
cat > ${TMPD}/cnt_build.sh << "EOF"
#!/bin/bash
# cnt_build.sh <image> <repo>: /var/lib/cloonix/bulk/<image>.tar.gz, as root.
set -e
IMAGE=$1
REPO=$2
DISTRO="trixie"
CNTREPO="http://10.117.0.1/trixie"
ROOTFS="/var/lib/cloonix/bulk/${IMAGE}"
# adduser: minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,"
LIST+="kmod,tcpdump,iw,hostapd,wpasupplicant,"
case ${IMAGE} in
  frr_trixie)      LIST+="frr,frr-pythontools" ; EXTRA="" ;;
  bird_trixie)     LIST+="bird2,curl,python3" ; EXTRA="iperf3 netcat-openbsd psmisc mtr-tiny" ;;
  suricata_trixie) LIST+="suricata,curl,python3" ; EXTRA="netcat-openbsd psmisc ethtool jq" ;;
  dns_trixie)      LIST+="frr,frr-pythontools,bind9,bind9-utils,bind9-dnsutils"
                   EXTRA="ldnsutils knot-dnsutils dnstracer whois netcat-openbsd python3" ;;
  *)               echo "ERROR unknown image ${IMAGE}" ; exit 1 ;;
esac
fct_chroot() {
unshare --mount --wd "${ROOTFS}" -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
if [ -e ${ROOTFS} ] || [ -e ${ROOTFS}.tar.gz ]; then
  echo "ERROR ${ROOTFS} or ${ROOTFS}.tar.gz already exists"
  exit 1
fi
mkdir -p /var/lib/cloonix/bulk
/usr/bin/mmdebstrap --variant=minbase --include=${LIST} \
      --aptopt='Acquire::AllowInsecureRepositories "true"' \
      --aptopt='APT::Get::AllowUnauthenticated "true"' \
      ${DISTRO} ${ROOTFS} \
      "deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main"
if [ ! -x ${ROOTFS}/usr/bin/bash ]; then
  echo "ERROR mmdebstrap did not produce a usable rootfs in ${ROOTFS}"
  exit 1
fi
# Optional packages: one by one, a missing one is reported, not fatal.
GOT=""
MISSING=""
if [ -n "${EXTRA}" ]; then
  fct_chroot "cat > /etc/apt/sources.list <<END
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main
END"
  # policy-rc.d exit 101: no init in this chroot, postinsts must not start daemons.
  fct_chroot "printf \"#!/bin/sh\nexit 101\n\" > /usr/sbin/policy-rc.d ; chmod 755 /usr/sbin/policy-rc.d"
  APT="DEBIAN_FRONTEND=noninteractive apt-get -o Acquire::AllowInsecureRepositories=true"
  APT="${APT} -o APT::Get::AllowUnauthenticated=true -o APT::Sandbox::User=root -y"
  if fct_chroot "${APT} update > /tmp/apt_extra.log 2>&1"; then
    for p in ${EXTRA}; do
      if fct_chroot "${APT} --no-install-recommends install ${p} >> /tmp/apt_extra.log 2>&1"; then
        GOT="${GOT} ${p}"
      else
        MISSING="${MISSING} ${p}"
      fi
    done
  else
    echo "  WARNING ${IMAGE}: apt-get update failed in the chroot, not attempted -${EXTRA}"
    sed -e "s/^/          /" ${ROOTFS}/tmp/apt_extra.log | tail -n 12
  fi
  fct_chroot "rm -f /usr/sbin/policy-rc.d /tmp/apt_extra.log ; apt-get clean"
fi
fct_chroot "cat > /etc/apt/sources.list <<END
deb [ trusted=yes allow-insecure=yes ] ${CNTREPO} ${DISTRO} main
END"
rm -rf ${ROOTFS}/var/lib/apt/lists/partial ${ROOTFS}/var/cache/apt/archives/partial
ln -sf /usr/bin/bash ${ROOTFS}/usr/bin/sh
# Real root made device nodes: an unprivileged tar cannot recreate them, crun gives /dev.
find ${ROOTFS}/dev -mindepth 1 -delete
chmod 755 ${ROOTFS}/dev ${ROOTFS}/proc ${ROOTFS}/sys
chmod 700 ${ROOTFS}/root
# rsyslog, locale and hosts: written into the image, not at every boot.
mkdir -p ${ROOTFS}/var/spool/rsyslog
cat > ${ROOTFS}/etc/rsyslog.conf << "END"
module(load="imuxsock")
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
END
echo "en_US.UTF-8 UTF-8" > ${ROOTFS}/etc/locale.gen
echo "LANG=C" > ${ROOTFS}/etc/locale.conf
echo "127.0.0.1   localhost" > ${ROOTFS}/etc/hosts
# Guest userns: files of an unmapped owner are claimed for uid 0.
case ${IMAGE} in
  frr_trixie|dns_trixie)
    for i in /etc/frr /var/log/frr /var/lib/frr /var/tmp/frr ; do
      if [ -d ${ROOTFS}${i} ]; then chown -R 0:0 ${ROOTFS}${i} ; fi
    done
    chmod 755 ${ROOTFS}/etc/frr
    # Runtime dirs world writable (lab image).
    for i in /var/log/frr /var/lib/frr /var/tmp/frr ; do
      if [ -d ${ROOTFS}${i} ]; then chmod 1777 ${ROOTFS}${i} ; fi
    done
    ;;
esac
case ${IMAGE} in
  frr_trixie)
    . /root/cnt_pola.sh
    cnt_pola ${ROOTFS}
    ;;
  bird_trixie)
    mkdir -p ${ROOTFS}/etc/bird ${ROOTFS}/var/log/bird ${ROOTFS}/run/bird
    CLAIM="/etc/bird /var/log/bird /run/bird /var/lib/bird"
    ;;
  suricata_trixie)
    mkdir -p ${ROOTFS}/etc/suricata/rules ${ROOTFS}/var/lib/suricata/rules
    mkdir -p ${ROOTFS}/var/log/suricata
    CLAIM="/etc/suricata /var/lib/suricata /var/log/suricata /run/suricata"
    ;;
  dns_trixie)
    CLAIM="/etc/bind /var/cache/bind /var/lib/bind /var/log/named"
    ;;
esac
for i in ${CLAIM}; do
  if [ -e ${ROOTFS}${i} ]; then
    chown -R 0:0 ${ROOTFS}${i}
    chmod -R u+rwX ${ROOTFS}${i}
  fi
done
if [ -n "${GOT}" ]; then echo "  ${IMAGE}: also installed -${GOT}" ; fi
if [ -n "${MISSING}" ]; then echo "  WARNING ${IMAGE}: not in the mirror, skipped -${MISSING}" ; fi
tar --numeric-owner -C ${ROOTFS} -czf ${ROOTFS}.tar.gz.tmp .
mv -f ${ROOTFS}.tar.gz.tmp ${ROOTFS}.tar.gz
rm -rf ${ROOTFS}
echo "  ${IMAGE}.tar.gz: $(du -sh ${ROOTFS}.tar.gz | cut -f1)"
EOF
cloonix cli ${NET} scp ${TMPD}/cnt_build.sh ${NAME}:/root || exit 1
rm -rf ${TMPD}
cloonix cli ${NET} scp ${HERE}/../../vm_create/cnt_kvm_demo/cnt_pola.sh \
  ${NAME}:/root || exit 1
for IMAGE in frr_trixie bird_trixie suricata_trixie dns_trixie; do
  cloonix cli ${NET} ssh ${NAME} "bash /root/cnt_build.sh ${IMAGE} ${REPO}" || \
    { echo "ERROR ${IMAGE}.tar.gz not built in ${NAME}"; exit 1; }
done
cloonix cli ${NET} ssh ${NAME} "rm -f /root/cnt_build.sh /root/cnt_pola.sh"
cloonix cli ${NET} ssh ${NAME} "sync"
cloonix cli ${NET} ssh ${NAME} "sync"
# Stopped from the host: poweroff in the guest cannot reach systemd from an
# ssh session of cloonix. The syncs above put builder.qcow2 on disk.
cloonix cli ${NET} del ${NAME}
