#!/bin/bash

set -x

NET=nemo
DIST=bullseye
PARAMS="ram=1000 cpu=4 eth=s"
NUM=20

#######################################################################
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
else
  cloonix_cli $NET rma
fi

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------

cloonix_cli $NET add nat nat1
cloonix_cli $NET add lan nat1 0 lan1
cloonix_cli $NET add tap top
cloonix_cli $NET add lan top 0 lan1
cloonix_cli $NET add a2b a2b1 
cloonix_cli $NET add lan a2b1 0 lan1

#######################################################################
for ((i=0;i<${NUM};i++)); do
  cloonix_cli $NET add kvm vm${i} ${PARAMS} ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------
for ((i=0;i<${NUM};i++)); do
  cloonix_cli $NET add lan vm${i} 0 lan1
done
#----------------------------------------------------------------------
for ((i=0;i<${NUM};i++)); do
  while ! cloonix_ssh $NET vm${i} "echo" 2>/dev/null; do
    echo vm${i} not ready, waiting 5 sec
    sleep 5
  done
done
#----------------------------------------------------------------------
for ((i=0;i<${NUM};i++)); do
  num=$((i+1))
  cloonix_ssh $NET vm${i} "ip addr add dev eth0 1.1.1.${num}/24"
  cloonix_ssh $NET vm${i} "ip link set dev eth0 up"
done
#----------------------------------------------------------------------
while [ 1 ]; do
  for ((i=0;i<${NUM};i++)); do
    num=$(((i+2)%${NUM}))
    cloonix_ssh $NET vm${i} "ping -c 3 1.1.1.${num}"
  done
done
#----------------------------------------------------------------------








