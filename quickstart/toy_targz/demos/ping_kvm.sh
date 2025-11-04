#!/bin/bash

NET="__IDENT__"

#----------------------------------------------------------------------
PARAMS="ram=1000 cpu=2 eth=s"
cloonix_cli ${NET} add kvm kvm1 ${PARAMS} trixie.qcow2 &
cloonix_cli ${NET} add kvm kvm2 ${PARAMS} trixie.qcow2 &
cloonix_cli ${NET} add lan kvm1 0 lan1
cloonix_cli ${NET} add lan kvm2 0 lan1
#----------------------------------------------------------------------
set +e
for i in "kvm1" "kvm2" ; do
  while ! cloonix_ssh ${NET} ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh ${NET} kvm1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh ${NET} kvm1 "ip link set dev eth0 up"
cloonix_ssh ${NET} kvm2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh ${NET} kvm2 "ip link set dev eth0 up"
cloonix_ssh ${NET} kvm1 "ping 1.1.1.2"
#----------------------------------------------------------------------



