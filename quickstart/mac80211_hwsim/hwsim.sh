#!/bin/bash

MIRROR=http://deb.debian.org/debian
DIST=buster
NET=nemo
VM_NAME=hwsim

PKT_LIST="hostapd wpasupplicant tcpdump"

source vm_construct.sh

set +e
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 10 sec for cleaning
  sleep 10
fi
cloonix_net $NET
sleep 1
cloonix_gui $NET
set -e

#######################################################################
CLOONIX_CONFIG=/usr/local/bin/cloonix/cloonix_config
CLOONIX_BULK=$(cat $CLOONIX_CONFIG |grep CLOONIX_BULK | awk -F = "{print \$2}")
BULK=$(eval echo $CLOONIX_BULK)
if [ ! -e ${BULK}/${VM_NAME}.qcow2 ]; then
    echo Virtual machine qcow2:
    echo ${BULK}/${VM_NAME}.qcow2
    echo Will be created.
    vm_construct $MIRROR $DIST $NET $BULK $VM_NAME $PKT_LIST
    sleep 5
fi

#######################################################################
for i in one two three; do
  cloonix_cli ${NET} add kvm ${i} ram=2048 cpu=2 dpdk=0 sock=0 hwsim=3 ${VM_NAME}.qcow2 & 
done
#----------------------------------------------------------------------
sleep 5
#----------------------------------------------------------------------
cloonix_cli ${NET} add lan one 0 wlan
cloonix_cli ${NET} add lan two 1 wlan
cloonix_cli ${NET} add lan three 2 wlan
#----------------------------------------------------------------------


for i in one two three ; do
#######################################################################
set +e
while ! cloonix_ssh ${NET} ${i} "echo" 2>/dev/null; do
  echo ${i} not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------
cloonix_scp ${NET} hostapd.conf ${i}:
cloonix_scp ${NET} wpa_supplicant.conf ${i}:
#----------------------------------------------------------------------
cloonix_ssh ${NET} ${i} "rm /dev/random; ln -s /dev/urandom /dev/random"
#----------------------------------------------------------------------
done
sleep 1
urxvt -title one -e cloonix_ssh ${NET} one "hostapd /root/hostapd.conf" &
sleep 1
#----------------------------------------------------------------------
urxvt -title two -e cloonix_ssh ${NET} two "\
                          wpa_supplicant -Dnl80211 -iwlan1 \
                          -c /root/wpa_supplicant.conf" &
#----------------------------------------------------------------------
urxvt -title three -e cloonix_ssh ${NET} three "\
                          wpa_supplicant -Dnl80211 -iwlan2 \
                          -c /root/wpa_supplicant.conf" &
#----------------------------------------------------------------------

cloonix_ssh ${NET} one "ifconfig wlan0 1.1.1.1/24 up"
cloonix_ssh ${NET} two "ifconfig wlan1 1.1.1.2/24 up"
cloonix_ssh ${NET} three "ifconfig wlan2 1.1.1.3/24 up"
cloonix_ssh ${NET} one "ping 1.1.1.3"
#----------------------------------------------------------------------





