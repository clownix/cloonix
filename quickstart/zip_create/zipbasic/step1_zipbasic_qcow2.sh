#!/bin/bash
#----------------------------------------------------------------------------#
DISTRO="trixie"
FROM="trixie.qcow2"
REPO="http://172.17.0.2/debian/${DISTRO}"
#REPO="http://deb.debian.org/debian"
QCOW2="zipbasic.qcow2"
#----------------------------------------------------------------------------#
set +e
if [ ! -e /var/lib/cloonix/bulk/${FROM} ]; then
  echo ${DISTRO}.qcow2 does not exist!
  exit 1
fi
is_started=$(cloonix_cli nemo pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli nemo kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
#----------------------------------------------------------------------------#
cloonix_net nemo
sleep 2
set -e
cp -vf /var/lib/cloonix/bulk/${FROM} /var/lib/cloonix/bulk/${QCOW2}
sync
#----------------------------------------------------------------------------#
cloonix_cli nemo add kvm vm ram=3000 cpu=2 eth=v ${QCOW2} --persistent
while ! cloonix_ssh nemo vm "echo"; do
  echo vm not ready, waiting 3 sec
  sleep 3
done
#----------------------------------------------------------------------------#
cloonix_ssh nemo vm "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main contrib non-free non-free-firmware
EOF"
#----------------------------------------------------------------------------#
cloonix_cli nemo add nat nat
cloonix_cli nemo add lan nat 0 lan_nat
cloonix_cli nemo add lan vm 0 lan_nat
cloonix_ssh nemo vm "dhcpcd eth0"
#----------------------------------------------------------------------------#
cloonix_ssh nemo vm "DEBIAN_FRONTEND=noninteractive \
                     DEBCONF_NONINTERACTIVE_SEEN=true \
                     apt-get -o Dpkg::Options::=--force-confdef \
                     -o Acquire::Check-Valid-Until=false \
                     --allow-unauthenticated --assume-yes update"
#----------------------------------------------------------------------------#
cloonix_ssh nemo vm "DEBIAN_FRONTEND=noninteractive \
                     DEBCONF_NONINTERACTIVE_SEEN=true \
                     apt-get -o Dpkg::Options::=--force-confdef \
                     -o Acquire::Check-Valid-Until=false \
                     --allow-unauthenticated --assume-yes install \
                     arping rsyslog gcc zip iperf3 strace mawk coreutils \
                     diffutils findutils grep hostname procps mount \
                     net-tools sed binutils tar debianutils file lsof \
                     libc-bin bash xkb-data iputils-ping iproute2 \
                     openssh-client openssh-server daemonize psmisc \
                     ncurses-term gzip x11-apps xauth bash-completion \
                     curl iputils-tracepath iptables tcpdump wget bsdutils \
                     util-linux passwd acl frr libyang3"
#----------------------------------------------------------------------------#
cloonix_scp nemo ./ldd_list_cp.c vm:~ 
cloonix_ssh nemo vm "gcc -o ldd_list_cp ldd_list_cp.c"
#----------------------------------------------------------------------------#
cloonix_ssh nemo vm "sync"
sleep 2
cloonix_ssh nemo vm "sync"
sleep 2
set +e
cloonix_ssh nemo vm "systemctl start poweroff.target"
sleep 5
cloonix_cli nemo kil
echo END
#----------------------------------------------------------------------------#

