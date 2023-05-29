#!/bin/bash

PARAMS="ram=1000 cpu=2 eth=ss"
NET=nemo
DIST=bookworm

#######################################################################
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
else
  cloonix_cli $NET rma
fi

echo waiting 2 sec
sleep 2

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------

#######################################################################
for i in 1 2 ; do
  cloonix_cli $NET add kvm Cloon${i} ${PARAMS} ${DIST}.qcow2 &
done
#----------------------------------------------------------------------
sleep 4
#----------------------------------------------------------------------
for i in 1 2 ; do
  cloonix_cli $NET add lan Cloon${i} 0 lan1
done
#######################################################################
set +e
for i in 1 2 ; do
  while ! cloonix_ssh $NET Cloon${i} "echo" 2>/dev/null; do
    echo Cloon${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------

#######################################################################
for i in 1 2 ; do
  cloonix_ssh $NET Cloon${i} "ip addr add dev eth0 1.1.1.${i}/24"
  cloonix_ssh $NET Cloon${i} "ip link set dev eth0 up"
done
#----------------------------------------------------------------------
cloonix_ssh $NET Cloon1 "ping 1.1.1.2"
#----------------------------------------------------------------------



