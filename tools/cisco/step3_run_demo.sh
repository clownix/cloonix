#!/bin/bash
HERE=`pwd`
NET=nemo
LINUX=buster
CISCO=cisco0
LIST_CISCO="cisco1 cisco2 cisco3" 
LIST_LINUX="linux1 linux2" 
BULK=${HOME}/cloonix_data/bulk
#######################################################################
for i in ${BULK}/${CISCO}.qcow2 ; do
  if [ ! -e ${i} ]; then
    echo ${i} not found
    exit 1
  fi
done
#######################################################################
if [ ! -e ${BULK}/${LINUX}.qcow2 ]; then
  echo Missing ${LINUX}.qcow2
  echo wget http://clownix.net/downloads/cloonix-04-03/bulk/buster.qcow2.gz
  echo mv buster.qcow2.gz ${BULK}
  echo cd ${BULK}
  echo gunzip buster.qcow2.gz
  exit
fi
#######################################################################
set +e
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 10 sec for cleaning
  sleep 10
fi
cloonix_net ${NET}
sleep 1
set -e
#######################################################################
fct_layout()
{
cloonix_cli ${NET} cnf lay width_height 700 420
sleep 1
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco3 3 270
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 0 151
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 1 233
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 2 76
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 3 269
cloonix_cli ${NET} cnf lay abs_xy_kvm linux2 300 130
cloonix_cli ${NET} cnf lay abs_xy_eth linux2 0 233
cloonix_cli ${NET} cnf lay abs_xy_eth linux2 1 76
cloonix_cli ${NET} cnf lay abs_xy_kvm linux1 -300 130
cloonix_cli ${NET} cnf lay abs_xy_eth linux1 0 76
cloonix_cli ${NET} cnf lay abs_xy_eth linux1 1 233
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco2 100 130
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 0 0
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 1 76
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 2 241
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 3 175
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco1 -100 130
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 0 305
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 1 233
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 2 70
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 3 128
cloonix_cli ${NET} cnf lay abs_xy_sat nat_cisco3 6 369
cloonix_cli ${NET} cnf lay abs_xy_sat nat_cisco2 95 9
cloonix_cli ${NET} cnf lay abs_xy_sat nat_cisco1 -107 11
cloonix_cli ${NET} cnf lay abs_xy_lan lan6 70 210
cloonix_cli ${NET} cnf lay abs_xy_lan lan5 -59 214
cloonix_cli ${NET} cnf lay abs_xy_lan lan3 0 125
cloonix_cli ${NET} cnf lay abs_xy_lan lan7 200 130
cloonix_cli ${NET} cnf lay abs_xy_lan lan1 -200 130
cloonix_cli ${NET} cnf lay abs_xy_lan lan_nat_cisco3 6 337
cloonix_cli ${NET} cnf lay abs_xy_lan lan_nat_cisco1 -106 54
cloonix_cli ${NET} cnf lay abs_xy_lan lan_nat_cisco2 98 52
cloonix_cli ${NET} cnf lay scale -1 166 800 480
}
#######################################################################


cloonix_gui ${NET}

#######################################################################
PARAMS="ram=2000 cpu=2 dpdk=0 sock=2 hwsim=0"
for i in ${LIST_LINUX} ; do
  cloonix_cli ${NET} add kvm ${i} ${PARAMS} ${LINUX}.qcow2 &
done


PARAMS="ram=5000 cpu=4 dpdk=0 sock=4 hwsim=0"
cloonix_cli ${NET} add kvm cisco1 ${PARAMS} ${CISCO}.qcow2 --cisco0 &
cloonix_cli ${NET} add kvm cisco2 ${PARAMS} ${CISCO}.qcow2 --cisco0 &
cloonix_cli ${NET} add kvm cisco3 ${PARAMS} cisco3.qcow2 --cisco3 &

sleep 30
#######################################################################
cloonix_cli ${NET} add lan linux1 0 lan1
cloonix_cli ${NET} add lan cisco1 1 lan1
#######################################################################
cloonix_cli ${NET} add lan linux2 0 lan7
cloonix_cli ${NET} add lan cisco2 1 lan7
#######################################################################
cloonix_cli ${NET} add lan cisco1 2 lan3
cloonix_cli ${NET} add lan cisco2 2 lan3
#######################################################################
cloonix_cli ${NET} add lan cisco1 3 lan5
cloonix_cli ${NET} add lan cisco3 1 lan5
#######################################################################
cloonix_cli ${NET} add lan cisco2 3 lan6
cloonix_cli ${NET} add lan cisco3 2 lan6
#######################################################################
fct_layout
#######################################################################
set +e
for i in $LIST_LINUX ; do
  while ! cloonix_ssh ${NET} ${i} "echo"; do
    echo ${i} not ready, waiting 3 sec
    sleep 3
  done
done
#######################################################################
for i in $LIST_CISCO ; do
  while [ 1 ]; do
    RET=$(cloonix_osh ${NET} ${i} ?)
    echo ${i} returned: $RET
    RET=${RET#*invalid }
    RET=${RET% *}
    if [ "${RET}" = "autocommand" ]; then
      echo
      echo ${i} is ready to receive
      echo
      break
    else
      echo ${i} not ready
      echo
    fi
    sleep 3
  done
done
set -e
#######################################################################
for i in $LIST_CISCO ; do
  cloonix_ocp ${NET} run_configs/${i}.cfg ${i}:running-config
done
#######################################################################
cloonix_ssh ${NET} linux1 "ip addr add dev eth0 1.0.0.1/24"
cloonix_ssh ${NET} linux1 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux1 "ip route add default via  1.0.0.2"
cloonix_ssh ${NET} linux2 "ip addr add dev eth0 5.0.0.1/24"
cloonix_ssh ${NET} linux2 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux2 "ip route add default via  5.0.0.2"
#######################################################################
cloonix_ssh ${NET} linux1 "ping 5.0.0.1"
#######################################################################

