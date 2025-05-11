#!/bin/bash
#----------------------------------------------------------------------------#
NET="__IDENT__"
FRRZIP="zipfrr.zip"
#----------------------------------------------------------------------
cloonix_cli $NET add zip cnt1 eth=s $FRRZIP
cloonix_cli $NET add zip cnt2 eth=s $FRRZIP
cloonix_cli $NET add lan cnt1 0 lan1
cloonix_cli $NET add lan cnt2 0 lan1
#----------------------------------------------------------------------
set +e
for i in cnt1 cnt2 ; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready yet, waiting 1 sec
    sleep 1
  done
done
#----------------------------------------------------------------------
set -e
#----------------------------------------------------------------------
cloonix_ssh $NET cnt1 "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh $NET cnt1 "ip link set dev eth0 up"
cloonix_ssh $NET cnt2 "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh $NET cnt2 "ip link set dev eth0 up"
cloonix_ssh $NET cnt1 "ping 1.1.1.2"
#----------------------------------------------------------------------



