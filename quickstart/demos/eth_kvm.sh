#!/bin/bash

#----------------------------------------------------------------------
cloonix_net nemo
#----------------------------------------------------------------------
cloonix_gui nemo
#----------------------------------------------------------------------
PARAMS="ram=1000 cpu=2 eth=s"
cloonix_cli nemo add kvm kvm1 ${PARAMS} trixie.qcow2 &
cloonix_cli nemo add kvm kvm2 ${PARAMS} trixie.qcow2 &
cloonix_cli nemo add lan kvm1 0 lan1
cloonix_cli nemo add lan kvm2 0 lan1
#----------------------------------------------------------------------
set +e
for i in kvm1 kvm2 ; do
  while ! cloonix_ssh nemo ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh nemo kvm1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh nemo kvm1 "ip link set dev eth0 up"
cloonix_ssh nemo kvm2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh nemo kvm2 "ip link set dev eth0 up"
cloonix_ssh nemo kvm1 "ping 1.1.1.2"
#----------------------------------------------------------------------



