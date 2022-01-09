#!/bin/bash
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
#----------------------------------------------------------------------

#######################################################################
for i in vm1 vm2; do
  cloonix_cli nemo add cnt $i eth=v bookworm.img
done
#----------------------------------------------------------------------
cloonix_cli $NET add lan vm1 0 lan
cloonix_cli $NET add lan vm2 0 lan
#----------------------------------------------------------------------
sleep 2
#----------------------------------------------------------------------
sudo crun exec vm1 ifconfig eth0 1.1.1.1/24
sudo crun exec vm2 ifconfig eth0 1.1.1.2/24
#----------------------------------------------------------------------

#######################################################################
while [ 1 ]; do
  sudo crun exec vm1 ping -c 3 1.1.1.2
  sleep 1
done
#----------------------------------------------------------------------




