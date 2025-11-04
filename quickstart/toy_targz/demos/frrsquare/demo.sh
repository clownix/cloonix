#!/bin/bash
set -e
#----------------------------------------------------------------------------#
NET="__IDENT__"
#----------------------------------------------------------------------------#
cp /root/demos/frrsquare/cloonixsbininitreplace /sbin
#----------------------------------------------------------------------------#
for i in "11" "21" "31" "12" "22" "32" "13" "23" "33" ; do
  cloonix_cli $NET add cvm cnt${i} eth=ssss  self_rootfs --startup_env="NODE_ID=cnt${i}"
  X=${i:0:1}
  Y=${i:1:1}
  LAN0="lan$((X)).$((Y-1)).1"
  LAN1="lan$((X)).$((Y)).0"
  LAN2="lan$((X)).$((Y)).1"
  LAN3="lan$((X-1)).$((Y)).0"
  cloonix_cli $NET add lan cnt${i} 0 $LAN0
  cloonix_cli $NET add lan cnt${i} 1 $LAN1
  cloonix_cli $NET add lan cnt${i} 2 $LAN2
  cloonix_cli $NET add lan cnt${i} 3 $LAN3
done
#----------------------------------------------------------------------------#

