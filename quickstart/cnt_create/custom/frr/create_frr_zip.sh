#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
ROOTNM=frr
QCOW2=create_frr.qcow2
DISTRO=trixie
REPO="http://deb.debian.org/debian"
#######################################################################
set +e
if [ ! -e /var/lib/cloonix/bulk/${DISTRO}_amd64.qcow2 ]; then
  echo ${DISTRO}_amd64.qcow2 does not exist!
  exit 1
fi
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
cp -vf /var/lib/cloonix/bulk/${DISTRO}_amd64.qcow2 /var/lib/cloonix/bulk/${QCOW2}
sync
sleep 1
sync
#######################################################################
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${ROOTNM} ${PARAMS} ${QCOW2} --persistent
#######################################################################
while ! cloonix_ssh ${NET} ${ROOTNM} "echo"; do
  echo ${ROOTNM} not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${ROOTNM} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main contrib non-free non-free-firmware
EOF"
#-----------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${ROOTNM} 0 lan_nat
cloonix_ssh ${NET} ${ROOTNM} "dhclient eth0"

#######################################################################
cloonix_ssh ${NET} ${ROOTNM} "DEBIAN_FRONTEND=noninteractive \
                              DEBCONF_NONINTERACTIVE_SEEN=true \
                              apt-get -o Dpkg::Options::=--force-confdef \
                              -o Acquire::Check-Valid-Until=false \
                              --allow-unauthenticated --assume-yes update"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${ROOTNM} "DEBIAN_FRONTEND=noninteractive \
                              DEBCONF_NONINTERACTIVE_SEEN=true \
                              apt-get -o Dpkg::Options::=--force-confdef \
                              -o Acquire::Check-Valid-Until=false \
                              --allow-unauthenticated --assume-yes install \
                              busybox rsyslog gcc zip iperf3 frr file \
                              strace"
#-----------------------------------------------------------------------#
cloonix_scp ${NET} ${HERE}/inside_frr_zip.sh ${ROOTNM}:/root
cloonix_scp ${NET} ${HERE}/../ldd_tool/lddlist.c ${ROOTNM}:/root
cloonix_ssh ${NET} ${ROOTNM} "gcc -o cplibdep lddlist.c"
cloonix_ssh ${NET} ${ROOTNM} "./inside_frr_zip.sh"
cloonix_scp ${NET} ${ROOTNM}:/root/${ROOTNM}.zip /var/lib/cloonix/bulk
#-----------------------------------------------------------------------#
sleep 5
cloonix_ssh ${NET} ${ROOTNM} "sync"
sleep 1
cloonix_ssh ${NET} ${ROOTNM} "sync"
sleep 1
cloonix_ssh ${NET} ${ROOTNM} "poweroff"
sleep 5
set +e
cloonix_cli ${NET} kil
sleep 1
echo END ################################################"
#-----------------------------------------------------------------------#

