#!/bin/bash
PARAMS="ram=2000 cpu=2 eth=d"
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
cloonix_cli $NET add kvm vm ${PARAMS} ${DIST}.qcow2 & 
cloonix_cli $NET add tap tsttap
#----------------------------------------------------------------------

sleep 5

#######################################################################
set +e
while ! cloonix_ssh $NET vm "echo" 2>/dev/null; do
  echo vm not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------


#######################################################################
cloonix_ssh $NET vm "ip addr add dev eth0 172.21.0.1/24"
cloonix_ssh $NET vm "ip link set dev eth0 up"
sudo ip addr add dev tsttap 172.21.0.2/24
sudo ip link set dev tsttap up
#----------------------------------------------------------------------
sleep 1
#----------------------------------------------------------------------
cloonix_cli $NET add lan vm 0 lan1
cloonix_cli $NET add lan tsttap 0 lan1


while [ 1 ]; do

sleep 3
cloonix_ssh $NET vm "ping -c 3 172.21.0.2"
sleep 1
cloonix_cli $NET del lan tsttap 0 lan1
sleep 1
set +e
cloonix_ssh $NET vm "ping -c 1 -w 2 172.21.0.2"
set -e
cloonix_cli $NET add lan tsttap 0 lan1

sleep 3
cloonix_ssh $NET vm "ping -c 3 172.21.0.2"
sleep 1
cloonix_cli $NET del lan vm 0 lan1
cloonix_cli $NET del lan tsttap 0 lan1
sleep 1
set +e
cloonix_ssh $NET vm "ping -c 1 -w 2 172.21.0.2"
set -e
cloonix_cli $NET add lan vm 0 lan1
cloonix_cli $NET add lan tsttap 0 lan1


done


