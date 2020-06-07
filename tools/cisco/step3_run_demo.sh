#!/bin/bash
HERE=`pwd`
NET=nemo
LINUX=buster
LIST_CISCO="cisco1 cisco2 cisco3" 
LIST_LINUX="linux1 linux2" 
BULK=${HOME}/cloonix_data/bulk
TYPE=cisco4
#######################################################################
for i in ${BULK}/${TYPE}.qcow2 ; do
  if [ ! -e ${i} ]; then
    echo ${i} not found
    exit 1
  fi
done
#######################################################################
if [ ! -e ${BULK}/${LINUX}.qcow2 ]; then
  echo Missing ${LINUX}.qcow2
  echo wget http://clownix.net/downloads/cloonix-07-02/bulk/buster.qcow2.gz
  echo mv buster.qcow2.gz ${BULK}
  echo cd ${BULK}
  echo gunzip buster.qcow2.gz
  exit
fi
#######################################################################
set +e
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 10 sec for cleaning
  sleep 10
fi
cloonix_net ${NET}
sleep 1
set -e
#######################################################################

cloonix_gui ${NET}

#######################################################################
PARAMS="ram=2000 cpu=2 eth=vv"
#PARAMS="ram=2000 cpu=2 eth=ss"
for i in ${LIST_LINUX} ; do
  cloonix_cli ${NET} add kvm ${i} ${PARAMS} ${LINUX}.qcow2 &
done


PARAMS="ram=5000 cpu=4 eth=vvvvd"
#PARAMS="ram=5000 cpu=4 eth=sssss"
cloonix_cli ${NET} add kvm cisco1 ${PARAMS} ${TYPE}.qcow2 --${TYPE} &
cloonix_cli ${NET} add kvm cisco2 ${PARAMS} ${TYPE}.qcow2 --${TYPE} &
cloonix_cli ${NET} add kvm cisco3 ${PARAMS} ${TYPE}.qcow2 --${TYPE} &

sleep 30
#######################################################################
cloonix_cli ${NET} add lan linux1 0 lan1
cloonix_cli ${NET} add lan cisco1 1 lan1
#######################################################################
cloonix_cli ${NET} add lan linux2 0 lan7
cloonix_cli ${NET} add lan cisco2 1 lan7
#######################################################################
cloonix_cli ${NET} add lan cisco1 2 lan3
cloonix_cli ${NET} add lan cisco2 2 lan3
#######################################################################
cloonix_cli ${NET} add lan cisco1 3 lan5
cloonix_cli ${NET} add lan cisco3 1 lan5
#######################################################################
cloonix_cli ${NET} add lan cisco2 3 lan6
cloonix_cli ${NET} add lan cisco3 2 lan6
#######################################################################
set +e
for i in $LIST_LINUX ; do
  while ! cloonix_ssh ${NET} ${i} "echo"; do
    echo ${i} not ready, waiting 3 sec
    sleep 3
  done
done
#######################################################################
for i in $LIST_CISCO ; do
  while [ 1 ]; do
    RET=$(cloonix_osh ${NET} ${i} ?)
    echo ${i} returned: $RET
    RET=${RET#*invalid }
    RET=${RET% *}
    if [ "${RET}" = "autocommand" ]; then
      echo
      echo ${i} is ready to receive
      echo
      break
    else
      echo ${i} not ready
      echo
    fi
    sleep 3
  done
done
set -e
#######################################################################
for i in $LIST_CISCO ; do
  cloonix_ocp ${NET} run_configs/${i}.cfg ${i}:running-config
done
#######################################################################
cloonix_ssh ${NET} linux1 "ip addr add dev eth0 1.0.0.1/24"
cloonix_ssh ${NET} linux1 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux1 "ip route add default via  1.0.0.2"
cloonix_ssh ${NET} linux2 "ip addr add dev eth0 5.0.0.1/24"
cloonix_ssh ${NET} linux2 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux2 "ip route add default via  5.0.0.2"
#######################################################################
cloonix_ssh ${NET} linux1 "ping 5.0.0.1"
#######################################################################

