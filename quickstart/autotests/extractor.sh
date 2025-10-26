#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
BULK="/var/lib/cloonix/bulk"
EXTRACTOR="cloonix-toy-53.sh"
LIST="trixie_lxde trixie_gnome fedora42_kde"
#----------------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli ${NET} pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
#----------------------------------------------------------------------------#
cloonix_net ${NET}
cloonix_gui ${NET}
cloonix_cli ${NET} add tap nemotap1
cloonix_cli ${NET} add lan nemotap1 0 lan
sudo ifconfig nemotap1 1.1.1.1/24 up
#----------------------------------------------------------------------------#
for i in ${LIST}; do
  cloonix_cli $NET add kvm ${i} ram=5000 cpu=4 eth=s desktop_${i}.qcow2 &
  cloonix_cli $NET add lan ${i} 0 lan
done
#----------------------------------------------------------------------------
for i in ${LIST}; do
  while ! cloonix_ssh ${NET} ${i} "echo"; do
    echo $i not ready, waiting 3 sec
    sleep 3
  done
done
#----------------------------------------------------------------------------
cd ${BULK}
tar zcvf trixie.tar.gz trixie
#----------------------------------------------------------------------------
num=1
for i in ${LIST}; do
  num=$((num+1))
  cloonix_scp $NET ${HOME}/${EXTRACTOR} ${i}:/home/user
  cloonix_ssh $NET ${i} "chown user:user /home/user/${EXTRACTOR}"
  cloonix_ssh $NET ${i} "su -c 'chmod +x /home/user/${EXTRACTOR}' user"
  cloonix_ssh $NET ${i} "mkdir -p /var/lib/cloonix/bulk"
  cloonix_scp $NET ${BULK}/trixie.zip ${i}:/${BULK}
  cloonix_scp $NET ${BULK}/trixie.qcow2 ${i}:/${BULK}
  cloonix_scp $NET ${BULK}/trixie.tar.gz ${i}:/${BULK}
  cloonix_ssh $NET ${i} "cd ${BULK}; tar xvf trixie.tar.gz"
  cloonix_ssh $NET ${i} "rm ${BULK}/trixie.tar.gz"
  cloonix_ssh $NET ${i} "chown -R user:user ${BULK}/trixie"
  cloonix_ssh $NET ${i} "systemctl stop NetworkManager.service 1>/dev/null 2>&1"
  cloonix_ssh $NET ${i} "ip addr add dev eth0 1.1.1.${num}/24"
  cloonix_ssh $NET ${i} "ip link set dev eth0 up"
done
rm ${BULK}/trixie.tar.gz


