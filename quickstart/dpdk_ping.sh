#!/bin/bash
NET=nemo
DIST=buster

#######################################################################
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
  echo waiting 2 sec
  sleep 2
else
  cloonix_cli $NET rma
  echo waiting 15 sec
  sleep 15 
fi

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------


#######################################################################
for i in one two; do
  cloonix_cli $NET add kvm ${i} ram=2000 cpu=2 dpdk=1 sock=0 hwsim=0 ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------

sleep 0.1


#######################################################################
set +e
for i in one two; do
  echo $i
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------

#######################################################################
cloonix_cli $NET add dpdk_tap tap1
#----------------------------------------------------------------------

#######################################################################
for i in one two tap1; do
  echo cloonix_cli $NET add lan $i 0 ooo
  cloonix_cli $NET add lan $i 0 ooo
done
#----------------------------------------------------------------------

#######################################################################
echo cloonix_ssh $NET one "ip addr add dev eth0 11.11.11.1/24"
cloonix_ssh $NET one "ip addr add dev eth0 11.11.11.1/24"

echo cloonix_ssh $NET two "ip addr add dev eth0 11.11.11.2/24"
cloonix_ssh $NET two "ip addr add dev eth0 11.11.11.2/24"

echo sudo ip addr add dev tap1 11.11.11.3/24
sudo ip addr add dev tap1 11.11.11.3/24

for i in one two ; do
  echo cloonix_ssh $NET ${i} "ip link set dev eth0 up"
  cloonix_ssh $NET ${i} "ip link set dev eth0 up"
done
#----------------------------------------------------------------------

urxvt -title vm-vm  -e cloonix_ssh $NET one "ping 11.11.11.2" &
urxvt -title vm-tap -e cloonix_ssh $NET one "ping 11.11.11.3" &

#----------------------------------------------------------------------



