#!/bin/bash

NET=nemo
POD=localhost/busybox

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
for i in 1 2 ; do
  cloonix_cli $NET add pod Cnt${i} eth=ss ${POD} &
done
#----------------------------------------------------------------------
sleep 3
#----------------------------------------------------------------------
for i in 1 2 ; do
  cloonix_cli $NET add lan Cnt${i} 0 lan1
done
#----------------------------------------------------------------------

#######################################################################
set +e
for i in 1 2 ; do
  while ! cloonix_ssh $NET Cnt${i} "echo" 2>/dev/null; do
    echo Cnt${i} not ready, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------


#######################################################################
for i in 1 2 ; do
  cloonix_ssh $NET Cnt${i} "ip addr add dev eth0 1.1.1.${i}/24"
done
#----------------------------------------------------------------------
cloonix_ssh $NET Cnt1 "ping 1.1.1.2"
#----------------------------------------------------------------------



