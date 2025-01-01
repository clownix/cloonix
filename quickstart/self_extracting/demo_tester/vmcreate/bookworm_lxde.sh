#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET=nemo
#----------------------------------------------------------------------#

#----------------------------------------------------------------------#
VMNAME=pod
QCOW2=bookworm_lxde.qcow2
REPO="http://172.17.0.2/debian/bookworm"
#######################################################################
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
cp -vf /var/lib/cloonix/bulk/bookworm_fr.qcow2 /var/lib/cloonix/bulk/${QCOW2}
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
deb [ trusted=yes allow-insecure=yes ] ${REPO} bookworm main contrib non-free non-free-firmware
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
cloonix_ssh ${NET} ${VMNAME} "apt-get -o Dpkg::Options::=--force-confdef \
                         -o Acquire::Check-Valid-Until=false \
                         --allow-unauthenticated --assume-yes install \
                         rxvt-unicode openssh-client openssh-server \
                         binutils rsyslog lxde spice-vdagent uidmap"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get --assume-yes remove xscreensaver"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat >> /etc/lightdm/lightdm.conf << EOF
[SeatDefaults]
autologin-in-background=true
autologin-user=user
autologin-user-timeout=5
EOF"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/xdg/lxsession/LXDE/autostart << EOF
@lxpanel --profile LXDE
@pcmanfm --desktop --profile LXDE
@xset s noblank
@xset s off
@xset -dpms
EOF"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "systemctl disable networking.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable bluetooth.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable connman-wait-online.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable connman.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable dundee.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable ofono.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable e2scrub_reap.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable wpa_supplicant.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable remote-fs.target"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable apt-daily.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable apt-daily-upgrade.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-network-generator.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-networkd-wait-online.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable systemd-pstore.service"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable dpkg-db-backup.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable e2scrub_all.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable fstrim.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable logrotate.timer"
cloonix_ssh ${NET} ${VMNAME} "systemctl disable man-db.timer"
cloonix_ssh ${NET} ${VMNAME} "rm -f /etc/resolv.conf"
#-----------------------------------------------------------------------#
sleep 5
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "poweroff"
sleep 10
set +e
cloonix_cli ${NET} kil
sleep 1
echo END ################################################"
#-----------------------------------------------------------------------#

