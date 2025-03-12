#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"

# list="debian ubuntu fedora"

list="ubuntu"

#----------------------------------------------------------------------#
if [ ! -e "/home/perrier/cloonix_extractor.sh" ]; then
  echo NOT FOUND:
  echo "/home/perrier/cloonix_extractor.sh"
  exit 1
fi
#----------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  echo ERASE $NET AND RELAUNCH
  exit
fi
cloonix_net $NET
set -e
cloonix_gui $NET
#----------------------------------------------------------------------------
for i in ${list}; do
  cloonix_cli $NET add kvm ${i} ram=3000 cpu=2 eth=s ${i}_iso.qcow2 &
done
#----------------------------------------------------------------------------
for i in ${list}; do
  while ! cloonix_ssh $NET ${i} "echo"; do
    echo $i not ready, waiting 3 sec
    sleep 3
  done
done
#----------------------------------------------------------------------------
for i in ${list}; do
  cloonix_scp $NET /home/perrier/cloonix_extractor.sh ${i}:/home/user
  cloonix_ssh $NET ${i} "chown user:user /home/user/cloonix_extractor.sh"
  cloonix_ssh $NET ${i} "su -c 'chmod +x /home/user/cloonix_extractor.sh' user"
done
#----------------------------------------------------------------------------

