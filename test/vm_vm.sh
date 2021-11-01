#!/bin/bash
PARAMS="ram=2000 cpu=2 eth=v"
NET=nemo
DIST=bullseye

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
for i in one two; do
  cloonix_cli $NET add kvm ${i} ${PARAMS} ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------

sleep 2

#######################################################################
cloonix_cli $NET add lan one 0 lan1
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
#----------------------------------------------------------------------


while [ 1 ]; do

sleep 3
cloonix_ssh $NET one "ping -c 3 11.11.11.2"
sleep 1
cloonix_cli $NET del lan one 0 lan1
sleep 1
set +e
cloonix_ssh $NET one "ping -c 1 -w 2 11.11.11.2"
set -e
cloonix_cli $NET add lan one 0 lan1

sleep 3
cloonix_ssh $NET one "ping -c 3 11.11.11.2"
sleep 1
cloonix_cli $NET del lan one 0 lan1
cloonix_cli $NET del lan two 0 lan1
sleep 1
set +e
cloonix_ssh $NET one "ping -c 1 -w 2 11.11.11.2"
set -e
cloonix_cli $NET add lan one 0 lan1
cloonix_cli $NET add lan two 0 lan1


done




