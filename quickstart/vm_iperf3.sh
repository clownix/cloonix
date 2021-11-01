#!/bin/bash


PARAMS="ram=2000 cpu=2 eth=sdv"
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

for i in 0 1 2 ; do
  cloonix_cli $NET add lan vm1 ${i} lan${i}
  cloonix_cli $NET add lan vm2 ${i} lan${i}
done

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
for i in 0 1 2 ; do
  cloonix_ssh $NET vm1 "ip addr add dev eth${i} $((i+1)).1.1.1/24"
  cloonix_ssh $NET vm2 "ip addr add dev eth${i} $((i+1)).1.1.2/24"
  cloonix_ssh $NET vm1 "ip link set dev eth${i} up"
  cloonix_ssh $NET vm2 "ip link set dev eth${i} up"
done
#----------------------------------------------------------------------
cloonix_ssh $NET vm2 "iperf3 -s" &
#----------------------------------------------------------------------
sleep 1
urxvt -title spyable -e cloonix_ssh $NET vm1 "iperf3 -c 1.1.1.2"
urxvt -title dpdk -e cloonix_ssh $NET vm1 "iperf3 -c 2.1.1.2"
urxvt -title vhost -e cloonix_ssh $NET vm1 "iperf3 -c 3.1.1.2"
kill $(jobs -p)
