#!/bin/bash

PARAMS="ram=2000 cpu=2 eth=s"
DIST=bullseye
NET=nemo

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
cloonix_cli $NET add kvm vm1 ${PARAMS} ${DIST}.qcow2 &
sleep 5
cloonix_cli $NET add kvm vm2 ${PARAMS} ${DIST}.qcow2 &
sleep 5
cloonix_cli $NET add a2b a2b
sleep 15
set +e
for i in vm1 vm2 ; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------

#######################################################################
cloonix_cli $NET add lan vm1 0 lan1
cloonix_cli $NET add lan vm2 0 lan2
cloonix_cli $NET add lan a2b 0 lan1
cloonix_cli $NET add lan a2b 1 lan2
#----------------------------------------------------------------------

#######################################################################
cloonix_ssh $NET vm1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET vm1 "ip link set dev eth0 up"
cloonix_ssh $NET vm2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh $NET vm2 "ip link set dev eth0 up"
#----------------------------------------------------------------------
echo
cloonix_ssh $NET vm1 "iperf3 -s" &
urxvt -title vm2 -e cloonix_ssh $NET vm2 "iperf3 -c 1.1.1.1"
kill $(jobs -p)
#----------------------------------------------------------------------
