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
cloonix_cli $NET add a2b a2b
#----------------------------------------------------------------------
sleep 3
#----------------------------------------------------------------------
for i in 0 1 ; do
  cloonix_cli $NET add lan Cloon$((i+1)) 0 lan${i}
  cloonix_cli $NET add lan a2b ${i} lan${i}
done
#######################################################################
set +e
for i in 1 2 ; do
  while ! cloonix_ssh $NET Cloon${i} "echo" 2>/dev/null; do
    echo Cloon${i} not ready, waiting 1 sec
    sleep 1
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
cloonix_cli $NET cnf a2b a2b "dir=0 cmd=loss val=5000"
cloonix_ssh $NET Cloon1 "ping -Q 0x44 1.1.1.2"
#----------------------------------------------------------------------



