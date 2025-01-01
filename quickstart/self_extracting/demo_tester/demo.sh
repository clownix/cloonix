#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
GNOME="bookworm_gnome.qcow2"
LXDE="bookworm_lxde.qcow2"
FEDO="fedora41_kde.qcow2"

for i in "11" "12" "13"; do
  if [ ! -e "/home/perrier/extract_vip${i}.sh" ]; then
    echo NOT FOUND:
    echo "/home/perrier/extract_vip${i}.sh"
    exit 1
  fi
done

set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  echo ERASE $NET AND RELAUNCH
  exit
fi
cloonix_net $NET
set -e
sleep 2
cloonix_gui $NET
#----------------------------------------------------------------------------
cloonix_cli $NET add kvm vip11 ram=3000 cpu=2 eth=s $GNOME &
cloonix_cli $NET add kvm vip12 ram=3000 cpu=2 eth=s $LXDE &
cloonix_cli $NET add kvm vip13 ram=3000 cpu=2 eth=s $FEDO &
#----------------------------------------------------------------------------
for i in "11" "12" "13"; do
  cloonix_cli $NET add lan vip$i 0 lan 
done
#----------------------------------------------------------------------------
for i in "11" "12" "13"; do
  while ! cloonix_ssh $NET vip$i "echo"; do
    echo vip$i not ready, waiting 3 sec
    sleep 3
  done
done
#----------------------------------------------------------------------------
for i in "11" "12" "13"; do
  cloonix_scp $NET /home/perrier/extract_vip${i}.sh vip${i}:/home/user
  cloonix_ssh $NET vip${i} "chown user:user /home/user/extract_vip${i}.sh"
  cloonix_ssh $NET vip${i} "su -c 'chmod +x /home/user/extract_vip${i}.sh' user"
  cloonix_ssh $NET vip${i} "ifconfig eth0 1.1.1.${i}/24"
  cloonix_ssh $NET vip${i} "ifconfig eth0"
done
#----------------------------------------------------------------------------

