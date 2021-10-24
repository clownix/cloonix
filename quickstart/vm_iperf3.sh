#!/bin/bash
PARAMS="ram=2000 cpu=2 eth=s"
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
for i in 1 2; do
  cloonix_cli $NET add kvm vm${i} ${PARAMS} ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------


cloonix_cli $NET add lan vm1 0 lan
cloonix_cli $NET add lan vm2 0 lan

#######################################################################
set +e
for i in 1 2; do
  while ! cloonix_ssh $NET vm${i} "echo" 2>/dev/null; do
    echo vm${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------


#######################################################################
cloonix_ssh $NET vm1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET vm2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh $NET vm1 "ip link set dev eth0 up"
cloonix_ssh $NET vm2 "ip link set dev eth0 up"
#----------------------------------------------------------------------
echo
#----------------------------------------------------------------------
sleep 10
urxvt -title server -e cloonix_ssh $NET vm2 "iperf3 -s" &
sleep 2
urxvt -title client -e cloonix_ssh $NET vm1 "iperf3 -c 1.1.1.2 -t 10000" &
#----------------------------------------------------------------------

sleep 10000

kill $(jobs -p)

echo DONE
