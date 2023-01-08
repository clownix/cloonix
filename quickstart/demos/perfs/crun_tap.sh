#!/bin/bash

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
cloonix_cli $NET add cru Cloon1 eth=v ${DIST}.img
cloonix_cli $NET add tap tap1
sleep 1
cloonix_cli $NET add lan Cloon1 0 lan1
cloonix_cli $NET add lan tap1 0 lan1
sleep 1
#----------------------------------------------------------------------

#######################################################################
set +e
while ! cloonix_ssh $NET Cloon1 "echo" 2>/dev/null; do
  echo Cloon1 not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------

#######################################################################
sudo ip addr add dev tap1 1.1.1.2/24
cloonix_ssh $NET Cloon1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET Cloon1 "ip link set dev eth0 up"
#----------------------------------------------------------------------

urxvt -e cloonix_ssh $NET Cloon1 "iperf3 -s -1" &
sleep 1
iperf3 -c 1.1.1.1
#----------------------------------------------------------------------



