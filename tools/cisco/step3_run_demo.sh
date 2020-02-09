#!/bin/bash
HERE=`pwd`
NET=nemo
LINUX=buster
CISCO=csr
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
  echo wget http://clownix.net/downloads/cloonix-04-02/bulk/buster.qcow2.gz
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
cloonix_cli ${NET} cnf lay stop
sleep 2
cloonix_cli ${NET} cnf lay width_height 700 420
sleep 1
cloonix_cli ${NET} cnf lay abs_xy_kvm linux1   -300  130
cloonix_cli ${NET} cnf lay abs_xy_lan lan1   -200  130
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco1 -100  130
cloonix_cli ${NET} cnf lay abs_xy_lan lan3      0  100
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco2  100  130
cloonix_cli ${NET} cnf lay abs_xy_lan lan7    200  130
cloonix_cli ${NET} cnf lay abs_xy_kvm linux2    300  130
cloonix_cli ${NET} cnf lay abs_xy_kvm cisco3    3  270
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 1 235
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 2 0
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 3 157
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 1 78
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 2 0
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 3 157
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 1 235
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 2 78
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 3 0
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 4 157
cloonix_cli ${NET} cnf lay abs_xy_eth linux1 0 78
cloonix_cli ${NET} cnf lay abs_xy_eth linux1 1 235
cloonix_cli ${NET} cnf lay abs_xy_eth linux2 0 235
cloonix_cli ${NET} cnf lay abs_xy_eth linux2 1 78
cloonix_cli ${NET} cnf lay abs_xy_lan lan6  70 210
cloonix_cli ${NET} cnf lay abs_xy_lan lan5 -70 210
cloonix_cli ${NET} cnf lay abs_xy_sat nat 0 164
cloonix_cli ${NET} cnf lay abs_xy_lan lan0 0 184
cloonix_cli ${NET} cnf lay abs_xy_eth cisco1 0 110
cloonix_cli ${NET} cnf lay abs_xy_eth cisco2 0 198
cloonix_cli ${NET} cnf lay abs_xy_eth cisco3 0 0
sleep 5
cloonix_cli ${NET} cnf lay scale 0 100 800 480
}
#######################################################################


cloonix_gui ${NET}

#######################################################################
PARAMS="ram=2000 cpu=2 dpdk=0 sock=2 hwsim=0"
for i in ${LIST_LINUX} ; do
  cloonix_cli ${NET} add kvm ${i} ${PARAMS} ${LINUX}.qcow2 &
done


PARAMS="ram=5000 cpu=4 dpdk=0 sock=4 hwsim=0"
for i in $LIST_CISCO ; do
  cloonix_cli ${NET} add kvm ${i} ${PARAMS} ${CISCO}.qcow2 --cisco &
done

cloonix_cli ${NET} add nat nat
sleep 30
#######################################################################
cloonix_cli ${NET} add lan nat    0 lan0
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
  cloonix_cli ${NET} cnf eth  ${i} 0 "set_link 0"
done
for i in $LIST_CISCO ; do

  cloonix_cli ${NET} add lan ${i} 0 lan0
  cloonix_cli ${NET} cnf eth  ${i} 0 "set_link 1"
  sleep 5

  while [ 1 ]; do
    RET=$(cloonix_osh ${NET} nat csr@${i} ?)
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

  cloonix_cli ${NET} del lan ${i} 0 lan0

done
set -e
#######################################################################
for i in $LIST_CISCO ; do
  cloonix_cli ${NET} cnf eth ${i} 0 "set_promisc 0"
  cloonix_cli ${NET} add lan ${i} 0 lan0
done
#######################################################################
for i in $LIST_CISCO ; do
  cloonix_ocp ${NET} nat configs/${i}.cfg csr@${i}:running-config
done
#   cloonix_osh ${NET} nat csr@${i} <<EOF
# write
# reload
# 
# EOF
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

