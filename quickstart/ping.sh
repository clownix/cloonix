#!/bin/bash
NET=nemo
DIST=buster

#######################################################################
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
  echo waiting 2 sec
  sleep 2
else
  cloonix_cli $NET rma
  echo waiting 10 sec
  sleep 10
fi

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------

#######################################################################
for i in one two; do
  cloonix_cli $NET add kvm ${i} ram=1024 cpu=2 dpdk=0 sock=3 hwsim=0 ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------

sleep 10
#######################################################################
cloonix_cli $NET add snf sniffer
#----------------------------------------------------------------------

#######################################################################
cloonix_cli $NET add lan one 0 lan1
cloonix_cli $NET add lan sniffer 0 lan1
cloonix_cli $NET add lan two 0 lan1
#----------------------------------------------------------------------

#######################################################################
set +e
for i in one two; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------

#######################################################################
cloonix_ssh $NET one "ip addr add dev eth0 11.11.11.1/24"
cloonix_ssh $NET two "ip addr add dev eth0 11.11.11.2/24"
for i in one two ; do
  cloonix_ssh $NET ${i} "ip link set dev eth0 up"
done
#----------------------------------------------------------------------

#######################################################################
cloonix_ssh $NET one "ping 11.11.11.2"
#----------------------------------------------------------------------




