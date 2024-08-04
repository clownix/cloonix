#!/bin/bash

#----------------------------------------------------------------------
cloonix_net nemo
#----------------------------------------------------------------------
cloonix_gui nemo
#----------------------------------------------------------------------
cloonix_cli nemo add cru cnt1 eth=s zipbasic.zip
cloonix_cli nemo add cru cnt2 eth=s zipbasic.zip
cloonix_cli nemo add lan cnt1 0 lan1
cloonix_cli nemo add lan cnt2 0 lan1
#----------------------------------------------------------------------
set +e
for i in cnt1 cnt2 ; do
  while ! cloonix_ssh nemo ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh nemo cnt1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh nemo cnt1 "ip link set dev eth0 up"
cloonix_ssh nemo cnt2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh nemo cnt2 "ip link set dev eth0 up"
cloonix_ssh nemo cnt1 "ping 1.1.1.2"
#----------------------------------------------------------------------



