#!/bin/bash

PARAMS="ram=1000 cpu=2 eth=s"
NET=nemo

#######################################################################
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
else
  cloonix_cli $NET rma
fi
#----------------------------------------------------------------------
echo waiting 2 sec
sleep 2
#----------------------------------------------------------------------
cloonix_gui $NET
cloonix_cli $NET add kvm kvm1 ${PARAMS} bookworm.qcow2 &
cloonix_cli $NET add cru cnt1 eth=s busybox.zip &
cloonix_cli $NET add lan kvm1 0 lan1
cloonix_cli $NET add lan cnt1 0 lan1
#----------------------------------------------------------------------
set +e
for i in kvm1 cnt1 ; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh $NET kvm1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET kvm1 "ip link set dev eth0 up"
cloonix_ssh $NET cnt1 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh $NET cnt1 "ip link set dev eth0 up"
cloonix_ssh $NET kvm1 "ping 1.1.1.2"
#----------------------------------------------------------------------



