#!/bin/bash
set -e
#----------------------------------------------------------------------------#
NET="__IDENT__"
#----------------------------------------------------------------------------#
cp /root/demos/line/cloonixsbininitreplace /sbin
#----------------------------------------------------------------------------#
cloonix_cli $NET add cvm cnt1 eth=s  self_rootfs --startup_env="NODE_ID=cnt1"
cloonix_cli $NET add cvm cnt2 eth=ss self_rootfs --startup_env="NODE_ID=cnt2"
cloonix_cli $NET add cvm cnt3 eth=s  self_rootfs --startup_env="NODE_ID=cnt3"
#----------------------------------------------------------------------------#
cloonix_cli $NET add lan cnt1 0 lan1
cloonix_cli $NET add lan cnt2 0 lan1
cloonix_cli $NET add lan cnt2 1 lan2
cloonix_cli $NET add lan cnt3 0 lan2
#----------------------------------------------------------------------------#

