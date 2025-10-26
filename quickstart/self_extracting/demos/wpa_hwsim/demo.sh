#!/bin/bash
set -e
#----------------------------------------------------------------------------#
NET="__IDENT__"
#----------------------------------------------------------------------------#
cp /root/demos/wpa_hwsim/cloonixsbininitreplace /sbin
#----------------------------------------------------------------------------#
cloonix_cli $NET add cvm cnt1 self_rootfs --startup_env="NODE_ID=cnt1" --vwifi=1
cloonix_cli $NET add cvm cnt2 self_rootfs --startup_env="NODE_ID=cnt2" --vwifi=1
cloonix_cli $NET add cvm cnt3 self_rootfs --startup_env="NODE_ID=cnt3" --vwifi=1
#----------------------------------------------------------------------------#

