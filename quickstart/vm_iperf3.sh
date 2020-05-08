#!/bin/bash
TYPE=$1
case "${TYPE}" in
  "dpdk")
    PARAMS="ram=2000 cpu=2 eth=d"
    ;;
  "sock")
    PARAMS="ram=2000 cpu=2 eth=s"
    ;;
  "vhost")
    PARAMS="ram=2000 cpu=2 eth=v"
    ;;
  *)
    echo ERROR FIRST PARAM: ${TYPE} must be dpdk sock or vhost
    exit 1
esac

NET=nemo
DIST=buster

#######################################################################
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
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
for i in 1 2; do
  cloonix_cli $NET add kvm vm${i} ${PARAMS} ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------

cloonix_cli $NET add lan vm1 0 lan
cloonix_cli $NET add lan vm2 0 lan

sleep 5

#######################################################################
set +e
for i in 1 2; do
  while ! cloonix_ssh $NET vm${i} "echo" 2>/dev/null; do
    echo vm${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------


#######################################################################
cloonix_ssh $NET vm1 "ip addr add dev eth0 1.0.0.1/24"
cloonix_ssh $NET vm2 "ip addr add dev eth0 1.0.0.2/24"
cloonix_ssh $NET vm1 "ip link set dev eth0 up"
cloonix_ssh $NET vm2 "ip link set dev eth0 up"
#----------------------------------------------------------------------
sleep 1
echo
cloonix_ssh $NET vm1 "ping -c 5 1.0.0.2"
sleep 1
echo
cloonix_ssh $NET vm1 "ping -f -c 10000 1.0.0.2"
sleep 1
echo
#----------------------------------------------------------------------
urxvt -title server -e cloonix_ssh $NET vm2 "iperf3 -s" &
sleep 1
urxvt -title client -e cloonix_ssh $NET vm1 "iperf3 -c 1.0.0.2 -t 10000" &
#----------------------------------------------------------------------
