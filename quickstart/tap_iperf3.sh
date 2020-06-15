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
    echo ERROR FIRST PARAM: ${TYPE} must be dpdk sock vhost
    exit 1
esac

NET=nemo
DIST=buster

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
for i in 1 2; do
  cloonix_cli $NET add kvm vm${i} ${PARAMS} ${DIST}.qcow2 & 
  cloonix_cli $NET add tap tap${i}
done
sleep 2
for i in 1 2; do
  cloonix_cli $NET add lan vm${i}  0 lan${i}
  cloonix_cli $NET add lan tap${i} 0 lan${i}
done
#----------------------------------------------------------------------

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
for i in 1 2; do
  cloonix_ssh $NET vm${i} "ip addr add dev eth0 172.2${i}.0.1/24"
  cloonix_ssh $NET vm${i} "ip link set dev eth0 up"
  sudo ip addr add dev tap${i} 172.2${i}.0.2/24
  sudo ip link set dev tap${i} up
done
#----------------------------------------------------------------------
sleep 1
#----------------------------------------------------------------------
urxvt -title server2 -e cloonix_ssh $NET vm2 "iperf3 -s" &
urxvt -title server1 -e iperf3 -s &
sleep 1
echo
echo
urxvt -title client1 -e cloonix_ssh $NET vm1 "iperf3 -c 172.21.0.2 -t 10000" &
sleep 1
urxvt -title client2 -e iperf3 -c 172.22.0.1 -t 10000  &

sleep 20

kill $(jobs -p)

echo DONE
#----------------------------------------------------------------------
