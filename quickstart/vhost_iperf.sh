#!/bin/bash

PARAMS="ram=2000 cpu=2 eth=v"
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
#----------------------------------------------------------------------

#######################################################################
set +e
  while ! cloonix_ssh $NET vm1 "echo" 2>/dev/null; do
    echo vm1 not ready, waiting 5 sec
    sleep 5
  done
set -e
#----------------------------------------------------------------------
conf_rank=$(cloonix_cli nemo dmp |grep ^conf_rank |awk '{print $2}')
vm_id=$(cloonix_cli nemo dmp |grep ^vm1 |awk '{print $3}')
eth_num=0
vhost_ifname=vho_${conf_rank}_${vm_id}_${eth_num}
echo ifname for vm1 eth0 : ${vhost_ifname}
sudo ip addr add dev ${vhost_ifname} 172.21.0.2/24
sudo ip link set dev ${vhost_ifname} up
#######################################################################
cloonix_ssh $NET vm1 "ip addr add dev eth0 172.21.0.1/24"
cloonix_ssh $NET vm1 "ip link set dev eth0 up"
#----------------------------------------------------------------------
cloonix_ssh $NET vm1 "iperf3 -s" &
iperf3 -s &
sleep 1
echo
urxvt -title tohost -e cloonix_ssh $NET vm1 "iperf3 -c 172.21.0.2"
urxvt -title tovm -e iperf3 -c 172.21.0.1
kill $(jobs -p)
#----------------------------------------------------------------------
