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


#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------

#######################################################################
cloonix_cli $NET add kvm vm1 ${PARAMS} ${DIST}.qcow2 &
cloonix_cli $NET add tap tap001
cloonix_cli $NET add lan vm1  0 lan1
cloonix_cli $NET add lan tap001 0 lan1
#----------------------------------------------------------------------

sleep 5

#######################################################################
set +e
  while ! cloonix_ssh $NET vm1 "echo" 2>/dev/null; do
    echo vm1 not ready, waiting 5 sec
    sleep 5
  done
set -e
#----------------------------------------------------------------------


#######################################################################
cloonix_ssh $NET vm1 "ip addr add dev eth0 172.21.0.1/24"
cloonix_ssh $NET vm1 "ip link set dev eth0 up"
sudo ip addr add dev tap001 172.21.0.2/24
sudo ip link set dev tap001 up
#----------------------------------------------------------------------
sleep 1
#----------------------------------------------------------------------
urxvt -title server2 -e cloonix_ssh $NET vm1 "iperf3 -s" &
urxvt -title server1 -e iperf3 -s &
sleep 1
echo
echo
urxvt -title tohost -e cloonix_ssh $NET vm1 "iperf3 -c 172.21.0.2 -t 10000" &
sleep 1
urxvt -title tovm -e iperf3 -c 172.21.0.1 -t 10000  &

sleep 20

kill $(jobs -p)

echo DONE
#----------------------------------------------------------------------
