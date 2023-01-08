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
cloonix_cli $NET add a2b a2b1
for i in 1 2 ; do
  cloonix_cli $NET add cru Cloon${i} eth=v ${DIST}.img
  sleep 1
  cloonix_cli $NET add lan Cloon${i} 0 lan${i}
done
#----------------------------------------------------------------------
cloonix_cli $NET add lan a2b1 0 lan1
cloonix_cli $NET add lan a2b1 1 lan2


#######################################################################
set +e
for i in 1 2 ; do
  while ! cloonix_ssh $NET Cloon${i} "echo" 2>/dev/null; do
    echo Cloon${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------

#######################################################################
for i in 1 2 ; do
  cloonix_ssh $NET Cloon${i} "ip addr add dev eth0 1.1.1.${i}/24"
  cloonix_ssh $NET Cloon${i} "ip link set dev eth0 up"
done
#----------------------------------------------------------------------

urxvt -e cloonix_ssh $NET Cloon2 "iperf3 -s -1" &
sleep 1
cloonix_ssh $NET Cloon1 "iperf3 -c 1.1.1.2"

#----------------------------------------------------------------------



