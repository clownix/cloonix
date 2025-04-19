#!/bin/bash
#----------------------------------------------------------------------------#
NET="nemo"
CVM="Cnt1"
#----------------------------------------------------------------------------#
set +e
if [ ! -e "/var/lib/cloonix/bulk/cvm_rootfs_orig" ]; then
  echo /var/lib/cloonix/bulk/cvm_rootfs_orig does not exist!
  exit 1
fi
is_started=$(cloonix_cli ${NET} pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
#----------------------------------------------------------------------------#
cloonix_net ${NET}
sleep 2
cloonix_gui ${NET}
set -e
#----------------------------------------------------------------------------#
cloonix_cli ${NET} add cvm ${CVM} eth=s cvm_rootfs_orig --persistent
while ! cloonix_ssh ${NET} ${CVM} "echo"; do
  echo ${CVM} not ready, waiting 3 sec
  sleep 3
done
#----------------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${CVM} 0 lan_nat
cloonix_ssh ${NET} ${CVM} "dhclient eth0"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes update"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes install \
                      accountsservice lxde openvswitch-switch \
                      virt-manager vlan sshpass curl xserver-xspice"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "sync"
sleep 2
set +e
cloonix_ssh ${NET} ${CVM} "poweroff"
sleep 5
cloonix_cli ${NET} kil
echo END
#----------------------------------------------------------------------------#

