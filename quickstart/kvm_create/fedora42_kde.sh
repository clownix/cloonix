#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
VMNAME="pod"
QCOW2="fedora42_kde.qcow2"
SRC="fedora42.qcow2"
BULK="/var/lib/cloonix/bulk"
#----------------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
#----------------------------------------------------------------------------#
cloonix_net ${NET}
sleep 2
set -e
cp -vf ${BULK}/${SRC} ${BULK}/${QCOW2}
sync
sleep 1
sync
#----------------------------------------------------------------------------#
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
#----------------------------------------------------------------------------#
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
#----------------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
cloonix_ssh ${NET} ${VMNAME} "dhclient eth0"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "dnf5 install -y @kde-desktop"
cloonix_ssh ${NET} ${VMNAME} "dnf5 install -y spice-vdagent"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable NetworkManager"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable bluetooth.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable remote-fs.target"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-network-generator.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-networkd-wait-online.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-pstore.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable fstrim.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable logrotate.timer"
cloonix_ssh ${NET} ${VMNAME} "rm -f /etc/resolv.conf"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "poweroff"
sleep 10
set +e
cloonix_cli ${NET} kil
sleep 1
echo END
#----------------------------------------------------------------------------#

