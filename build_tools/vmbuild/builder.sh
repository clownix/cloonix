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
is_started=$(cloonix cli $NET dump pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix start $NET:\n"
  cloonix start $NET
else
  cloonix cli $NET del all
fi
echo cp -f ${BULK}/${RAW} ${BULK}/${QCOW2}
cp -f ${BULK}/${RAW} ${BULK}/${QCOW2}
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
  util-linux tar unzip zip bzip2 curl wget rsync lsof strace gdb \
  python3 python3-venv python3-dev python3-pip python3-wheel \
  python3-setuptools python3-wheel python3-pyparsing python3-six \
  arping iproute2 tcpdump iputils-tracepath iperf3 daemonize gcc-14-plugin-dev \
  gcc-mingw-w64-x86-64 \
  clang lld llvm"
# The two cross toolchains: gcc-mingw-w64-x86-64 (windows agent), clang lld llvm
# (freebsd agent: only its linker and strip read a freebsd elf). build_cloonfs
# checks for them and stops rather than build an agent iso without its agent.
cloonix cli ${NET} ssh ${NAME} "sync"
cloonix cli ${NET} ssh ${NAME} "sync"
cloonix cli ${NET} ssh ${NAME} "poweroff"
