#!/bin/bash

NET=nemo
NET2=mito

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
is_started=$(cloonix_cli $NET2 pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET2:\n"
  cloonix_net $NET2
else
  cloonix_cli $NET2 rma
fi


echo waiting 2 sec
sleep 2

#######################################################################
cloonix_gui $NET
cloonix_gui $NET2
#----------------------------------------------------------------------

#######################################################################
cloonix_cli $NET  add c2c c2c1 $NET2
cloonix_cli $NET  add cru Cloon1 eth=v bookworm.img
cloonix_cli $NET2 add cru Cloon2 eth=v bullseye.img
sleep 1
cloonix_cli $NET  add lan Cloon1 0 lan1
cloonix_cli $NET2 add lan Cloon2 0 lan1
#----------------------------------------------------------------------
cloonix_cli $NET  add lan c2c1 0 lan1
cloonix_cli $NET2 add lan c2c1 0 lan1
#----------------------------------------------------------------------

#######################################################################
set +e
while ! cloonix_ssh $NET Cloon1 "echo" 2>/dev/null; do
  echo Cloon1 not ready, waiting 5 sec
  sleep 5
done
while ! cloonix_ssh $NET2 Cloon2 "echo" 2>/dev/null; do
  echo Cloon2 not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------

#######################################################################
cloonix_ssh $NET  Cloon1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET  Cloon1 "ip link set dev eth0 up"
cloonix_ssh $NET2 Cloon2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh $NET2 Cloon2 "ip link set dev eth0 up"
#----------------------------------------------------------------------

urxvt -e cloonix_ssh $NET Cloon1 "iperf3 -s -1" &
sleep 1
cloonix_ssh $NET2 Cloon2 "iperf3 -c 1.1.1.1"

#----------------------------------------------------------------------



