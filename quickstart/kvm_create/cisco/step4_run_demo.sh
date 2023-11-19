#!/bin/bash
HERE=`pwd`
NET=nemo
LINUX=bookworm
LIST_CISCO="cisco1 cisco2 cisco3" 
LIST_LINUX="linux1 linux2" 
NAME=c8000

#######################################################################
if [ ! -e /var/lib/cloonix/bulk/${NAME}.qcow2 ]; then
  echo ${NAME}.qcow2 not found
  exit 1
fi
#######################################################################
if [ ! -e /var/lib/cloonix/bulk/${LINUX}.qcow2 ]; then
  echo Missing ${LINUX}.qcow2
  exit
fi
#######################################################################
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 10 sec for cleaning
  sleep 10
fi
cloonix_net ${NET}
sleep 1
set -e
#######################################################################

cloonix_gui ${NET}

#######################################################################
PARAMS="ram=2000 cpu=2 eth=ss"
for i in ${LIST_LINUX} ; do
  cloonix_cli ${NET} add kvm ${i} ${PARAMS} ${LINUX}.qcow2 &
done


PARAMS="ram=5000 cpu=4 eth=ssss"
cloonix_cli ${NET} add kvm cisco1 ${PARAMS} ${NAME}.qcow2 --no_qemu_ga --natplug=0 &
cloonix_cli ${NET} add kvm cisco2 ${PARAMS} ${NAME}.qcow2 --no_qemu_ga --natplug=0 &
cloonix_cli ${NET} add kvm cisco3 ${PARAMS} ${NAME}.qcow2 --no_qemu_ga --natplug=0 &

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
set +e
count=0
for i in $LIST_CISCO ; do
  while [ 1 ]; do
    RET=$(cloonix_osh ${NET} ${i} ?)
    #echo ${i} returned: $RET
    RET=${RET#*invalid }
    RET=${RET% *}
    if [ "${RET}" = "autocommand" ]; then
      echo ${i} is ready to receive
      break
    else
      echo ${i} not ready $((count*10)) sec
    fi
    count=$((count+1))
    sleep 10
  done
done
#######################################################################
for i in $LIST_LINUX ; do
  while ! cloonix_ssh ${NET} ${i} "echo"; do
    echo ${i} not ready, waiting 3 sec
    sleep 3
  done
done
set -e
#######################################################################
cloonix_ssh ${NET} linux1 "ip addr add dev eth0 1.0.0.1/24"
cloonix_ssh ${NET} linux1 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux1 "ip route add default via  1.0.0.2"
cloonix_ssh ${NET} linux2 "ip addr add dev eth0 5.0.0.1/24"
cloonix_ssh ${NET} linux2 "ip link set dev eth0 up"
cloonix_ssh ${NET} linux2 "ip route add default via  5.0.0.2"
#######################################################################
for i in $LIST_CISCO ; do
  cloonix_ocp ${NET} configs/${i}.cfg ${i}:running-config
done
#######################################################################
cloonix_ssh ${NET} linux1 "ping 5.0.0.1"
#######################################################################

