#!/bin/bash

HERE=`pwd`
NET=nemo
LINUX=bullseye
BULK=${HOME}/cloonix_data/bulk
NAME=mikrotik

#######################################################################
for i in ${BULK}/${NAME}.qcow2 ; do
  if [ ! -e ${i} ]; then
    echo ${i} not found
    exit 1
  fi
done
#######################################################################
if [ ! -e ${BULK}/${LINUX}.qcow2 ]; then
  echo Missing ${LINUX}.qcow2
  exit
fi
#######################################################################
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
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
PARAMS="ram=2000 cpu=2 eth=ss"
cloonix_cli ${NET} add kvm linux1 ${PARAMS} ${LINUX}.qcow2 &
cloonix_cli ${NET} add kvm linux2 ${PARAMS} ${LINUX}.qcow2 &

PARAMS="ram=2000 cpu=4 eth=ssss"
cloonix_cli ${NET} add kvm mikro ${PARAMS} ${NAME}.qcow2 --nobackdoor --natplug=0 &

#######################################################################
sleep 10
for i in linux1 linux2 ; do
  while ! cloonix_ssh ${NET} ${i} "echo"; do
    echo ${i} not ready, waiting 3 sec
    sleep 3
  done
done
#######################################################################
cloonix_cli ${NET} add lan linux1 0 lan1
cloonix_cli ${NET} add lan mikro 1 lan1
#######################################################################
cloonix_cli ${NET} add lan linux2 0 lan2
cloonix_cli ${NET} add lan mikro 2 lan2
#######################################################################
set +e
while [ 1 ]; do
  RET=$(cloonix_osh ${NET} mikro quit)
  if [ "${RET}" = "interrupted" ]; then
    echo ${i} is ready to receive
    break
  else
    echo ${i} not ready $((count*3)) sec
  fi
  count=$((count+1))
  sleep 3
done
#######################################################################
cloonix_osh ${NET} mikro "ip address print"
#######################################################################

