#!/bin/bash
#----------------------------------------------------------------------#
if [ ${#} = 1 ]; then
  if [ "$1" = "i386" ]; then
    ARCH="i386"
    QCOW2="create_busybox_i386.qcow2"
  else
    echo param must be i386, if no param, then is amd64
    exit
  fi
else
  QCOW2="create_busybox_amd64.qcow2"
  ARCH="amd64"
fi
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
VMNAME=buz
DISTRO=bookworm
REPO="http://deb.debian.org/debian"
REPO="http://172.17.0.2/debian/${DISTRO}"
#######################################################################
set +e
if [ ! -e /var/lib/cloonix/bulk/${DISTRO}_${ARCH}.qcow2 ]; then
  echo ${DISTRO}_${ARCH}.qcow2 does not exist!
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
cp -vf /var/lib/cloonix/bulk/${DISTRO}_${ARCH}.qcow2 /var/lib/cloonix/bulk/${QCOW2}
sync
sleep 1
sync
#######################################################################
PARAMS="ram=3000 cpu=2 eth=v"
if [ ${ARCH} = "i386" ]; then
  cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent --i386
else
  cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
fi
#######################################################################
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main contrib non-free non-free-firmware
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
                              busybox rsyslog gcc zip iperf3"
#-----------------------------------------------------------------------#
cloonix_scp ${NET} -r ${HERE}/build_zip ${VMNAME}:/root
cloonix_scp ${NET} ${HERE}/../ldd_tool/lddlist.c ${VMNAME}:/root/build_zip
cloonix_ssh ${NET} ${VMNAME} "cd /root/build_zip; gcc -o cplibdep lddlist.c"
cloonix_ssh ${NET} ${VMNAME} "cd /root/build_zip; ./create_zip.sh ${ARCH}"
if [ ${ARCH} = "i386" ]; then
  cloonix_scp ${NET} ${VMNAME}:/root/busybox_i386.zip /var/lib/cloonix/bulk
else
  cloonix_scp ${NET} ${VMNAME}:/root/busybox.zip /var/lib/cloonix/bulk
fi
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

