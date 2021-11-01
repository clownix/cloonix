#!/bin/bash
PARAMS="ram=2000 cpu=2 eth=vds"
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
for i in vm1 vm2; do
  cloonix_cli $NET add kvm ${i} ${PARAMS} ${DIST}.qcow2 &
done
#----------------------------------------------------------------------

#----------------------------------------------------------------------
for i in 0 1 2 ; do
  cloonix_cli $NET add lan vm1 ${i} lan${i}
  cloonix_cli $NET add lan vm2 ${i} lan${i}
done


#######################################################################
set +e
for i in vm1 vm2; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 5 sec
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

#######################################################################
while [ 1 ]; do
  for i in 1 2 3 ; do
    cloonix_ssh $NET vm1 "ping -c 3 ${i}.1.1.2"
  done
  echo
  sleep 1
done
#----------------------------------------------------------------------




