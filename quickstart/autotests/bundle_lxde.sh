#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
BULK="/var/lib/cloonix/bulk"
BUNDLE="cloonix-bundle-53"
LIST="trixie_lxde"
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
  cloonix_cli $NET add kvm ${i} ram=5000 cpu=4 eth=s ${i}.qcow2 &
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
  cloonix_scp $NET ${HOME}/${BUNDLE}.tar.gz ${i}:/root
  cloonix_ssh $NET ${i} "tar xvf ${BUNDLE}.tar.gz"
  cloonix_ssh $NET ${i} "cd /root/${BUNDLE}; ./install_cloonfs"
  cloonix_ssh $NET ${i} "mkdir -p /var/lib/cloonix/bulk"
  cloonix_scp $NET ${BULK}/trixie.zip ${i}:/${BULK}
  cloonix_scp $NET ${BULK}/trixie.qcow2 ${i}:/${BULK}
  cloonix_scp $NET ${BULK}/trixie.tar.gz ${i}:/${BULK}
  cloonix_ssh $NET ${i} "cd ${BULK}; tar xvf trixie.tar.gz"
  cloonix_ssh $NET ${i} "rm ${BULK}/trixie.tar.gz"
  cloonix_ssh $NET ${i} "chown -R user:user ${BULK}/trixie"
  cloonix_ssh $NET ${i} "chmod 666 ${BULK}/trixie.qcow2"
  cloonix_ssh $NET ${i} "ip addr add dev eth0 1.1.1.${num}/24"
  cloonix_ssh $NET ${i} "ip link set dev eth0 up"
  cloonix_scp $NET -r ${HERE}/../demos ${i}:/home/user
  cloonix_ssh $NET ${i} "chown -R user:user /home/user/demos"
done
rm ${BULK}/trixie.tar.gz
#----------------------------------------------------------------------------
