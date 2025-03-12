#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`

NET="N4"
NAME="node4"
IP="1.1.1.4/24"

QCOW2="bookworm.qcow2"
set +e
is_started=$(cloonix_cli ${NET} pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
set -e
sleep 2
cloonix_gui ${NET}
cloonix_cli ${NET} add kvm ${NAME} ram=2000 cpu=2 eth=s ${QCOW2}
cloonix_cli ${NET} add lan ${NAME} 0 lan1
set +e
while ! cloonix_ssh ${NET} ${NAME} "echo"; do
  echo ${NAME} not ready, waiting 3 sec
  sleep 3
done
cloonix_cli ${NET} add c2c N43 N3
cloonix_cli ${NET} add lan N43 0 lan1
set -e
cloonix_ssh ${NET} ${NAME} "ifconfig eth0 ${IP}"
#----------------------------------------------------------------------#
