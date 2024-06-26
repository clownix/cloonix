#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
VMNAME=buz
QCOW2="podman_frr_cloonix.qcow2"
REPO="http://deb.debian.org/debian"
BOOKWORM="/var/lib/cloonix/bulk/trixie_amd64.qcow2"
BUNDLE_PATH="/home/perrier"
BUNDLE="cloonix-bundle-39-00-amd64"
#######################################################################
if [ ! -e ${BOOKWORM} ]; then
  echo NOT FOUND: ${BOOKWORM}
  exit 1
fi
if [ ! -e /var/lib/cloonix/bulk/frr.zip ]; then
  echo NOT FOUND: /var/lib/cloonix/bulk/frr.zip
  exit 1
fi
#----------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
set -e
cp -vf ${BOOKWORM} /var/lib/cloonix/bulk/${QCOW2}
sync
sleep 1
sync
#######################################################################
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
#######################################################################
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} trixie main contrib non-free non-free-firmware
EOF"
#-----------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
cloonix_ssh ${NET} ${VMNAME} "dhclient eth0"

#######################################################################
cloonix_ssh ${NET} ${VMNAME} "DEBIAN_FRONTEND=noninteractive \
                              DEBCONF_NONINTERACTIVE_SEEN=true \
                              apt-get -o Dpkg::Options::=--force-confdef \
                              -o Acquire::Check-Valid-Until=false \
                              --allow-unauthenticated --assume-yes update"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "DEBIAN_FRONTEND=noninteractive \
                              DEBCONF_NONINTERACTIVE_SEEN=true \
                              apt-get -o Dpkg::Options::=--force-confdef \
                              -o Acquire::Check-Valid-Until=false \
                              --allow-unauthenticated --assume-yes install \
                              busybox rsyslog gcc zip util-linux \
                              ncurses-term openssh-client openssh-server \
                              rsyslog bridge-utils x11-apps strace"
#-----------------------------------------------------------------------#
cloonix_scp ${NET} ${HERE}/inside_cloonix_zip.sh ${VMNAME}:/root
cloonix_scp ${NET} ${HERE}/spider_demo.sh ${VMNAME}:/root
cloonix_scp ${NET} /var/lib/cloonix/bulk/frr.zip ${VMNAME}:/root
cloonix_scp ${NET} ${HERE}/../ldd_tool/lddlist.c ${VMNAME}:/root
cloonix_scp ${NET} ${BUNDLE_PATH}/${BUNDLE}.tar.gz ${VMNAME}:~
cloonix_ssh ${NET} ${VMNAME} "tar xvf ${BUNDLE}.tar.gz"
cloonix_ssh ${NET} ${VMNAME} "cd ${BUNDLE}; ./install_cloonix"
cloonix_ssh ${NET} ${VMNAME} "rm -rf ${BUNDLE}"
cloonix_ssh ${NET} ${VMNAME} "gcc -o cplibdep lddlist.c"
cloonix_ssh ${NET} ${VMNAME} "./inside_cloonix_zip.sh"
cloonix_scp ${NET} ${VMNAME}:/root/podman_frr_cloonix.zip /var/lib/cloonix/bulk
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "sed -i s\"/#PermitRootLogin prohibit-password/PermitRootLogin yes/\" /etc/ssh/sshd_config"
cloonix_ssh ${NET} ${VMNAME} "sed -i s\"/#StrictModes yes/StrictModes no/\" /etc/ssh/sshd_config"
cloonix_ssh ${NET} ${VMNAME} "sed -i s\"/#IgnoreUserKnownHosts no/IgnoreUserKnownHosts yes/\" /etc/ssh/sshd_config"
cloonix_ssh ${NET} ${VMNAME} "systemctl enable rsyslog"
cloonix_ssh ${NET} ${VMNAME} "systemctl enable sshd"
#-----------------------------------------------------------------------#
sleep 5
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "poweroff"
sleep 5
set +e
cloonix_cli ${NET} kil
sleep 1
echo END ################################################"
#-----------------------------------------------------------------------#

