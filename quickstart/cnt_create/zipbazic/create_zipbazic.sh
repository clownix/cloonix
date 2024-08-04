#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
QCOW2="zipbasic.qcow2"
#----------------------------------------------------------------------------#
set +e
if [ ! -e /var/lib/cloonix/bulk/${QCOW2} ]; then
  echo ${QCOW2} does not exist!
  exit 1
fi
is_started=$(cloonix_cli nemo pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli nemo kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net nemo
sleep 2
#----------------------------------------------------------------------------#
set -e
cloonix_cli nemo add kvm vm ram=3000 cpu=2 eth=v ${QCOW2}
#----------------------------------------------------------------------------#
while ! cloonix_ssh nemo vm "echo"; do
  echo vm not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_scp nemo ${HERE}/build_zip.sh vm:/root
cloonix_ssh nemo vm /root/build_zip.sh
rm -f /var/lib/cloonix/bulk/zipbasic.zip
cloonix_scp nemo vm:/root/zipbasic.zip /var/lib/cloonix/bulk
#-----------------------------------------------------------------------#
cloonix_ssh nemo vm "sync"
sleep 2
set +e
cloonix_ssh nemo vm "systemctl start poweroff.target"
sleep 2
cloonix_cli nemo kil
echo END
#-----------------------------------------------------------------------#

