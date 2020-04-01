#!/bin/bash
TYPE=$1
case "${TYPE}" in
  "dpdk")
    PARAMS="ram=2000 cpu=2 dpdk=2 sock=0 hwsim=0"
    ;;
  "sock")
    PARAMS="ram=2000 cpu=2 dpdk=0 sock=2 hwsim=0"
    ;;
  *)
    echo ERROR FIRST PARAM: ${TYPE} must be either dpdk or sock
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
  echo waiting 2 sec
  sleep 2
else
  cloonix_cli $NET rma
  echo waiting 20 sec
  sleep 20 
fi

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------


#######################################################################
for i in 1 2 3; do
  cloonix_cli $NET add kvm vm${i} $PARAMS ${DIST}.qcow2 & 
done
#----------------------------------------------------------------------

sleep 5

#######################################################################
set +e
for i in 1 2 3; do
  while ! cloonix_ssh $NET vm${i} "echo" 2>/dev/null; do
    echo vm${i} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------


#######################################################################
for ((i=1;i<4;i++)); do
  cloonix_ssh $NET vm${i} "ip addr add dev eth0 $((i)).0.0.$((i))/24"
  cloonix_ssh $NET vm${i} "ip addr add dev eth1 $((i+1)).0.0.$((i))/24"
  cloonix_ssh $NET vm${i} "ip link set dev eth0 up"
  cloonix_ssh $NET vm${i} "ip link set dev eth1 up"
  cloonix_ssh $NET vm${i} "echo 1 > /proc/sys/net/ipv4/ip_forward"
  cloonix_cli $NET add lan vm${i} 1 lan$((i))
  cloonix_cli $NET add lan vm${i} 0 lan$((i-1))
done
cloonix_ssh $NET vm1 "ip route add default via 2.0.0.2"
cloonix_ssh $NET vm3 "ip route add default via 3.0.0.2"
cloonix_cli $NET del lan vm1 0 lan0
cloonix_cli $NET del lan vm3 1 lan3
#----------------------------------------------------------------------
sleep 1
echo
cloonix_ssh $NET vm1 "ping -c 5 3.0.0.3"
sleep 1
echo
cloonix_ssh $NET vm1 "ping -f -c 10000 3.0.0.3"
sleep 1
echo
#----------------------------------------------------------------------
urxvt -title server -e cloonix_ssh $NET vm3 "iperf3 -s" &
sleep 1
urxvt -title client -e cloonix_ssh $NET vm1 "iperf3 -c 3.0.0.3 -t 10000" &
#----------------------------------------------------------------------
