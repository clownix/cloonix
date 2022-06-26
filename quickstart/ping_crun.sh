#!/bin/bash

PARAMS="ram=2000 cpu=2 eth=ss"
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
for i in 1 2 ; do
  cloonix_cli $NET add cnt Crun${i} eth=ss ${DIST}.img
  cloonix_cli $NET add lan Crun${i} 0 lan1
done
#----------------------------------------------------------------------

sleep 2

for i in 1 2 ; do
  sudo crun exec Crun${i} ifconfig cnt0 1.1.1.${i}/24 up
done
#----------------------------------------------------------------------

sudo crun exec Crun1 ping 1.1.1.2

#----------------------------------------------------------------------



