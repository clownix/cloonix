#!/bin/bash
HERE=`pwd`
PARAMS="ram=5000 cpu=4 eth=s"
NET=nemo
# QCOW2=lxde_bookworm.qcow2
# QCOW2=lxde_bullseye_fr.qcow2

# QCOW2=bookworm.qcow2
# QCOW2=centos8.qcow2
# QCOW2=fedora37.qcow2
# QCOW2=jammy.qcow2
# QCOW2=opensuse155.qcow2
# QCOW2=tumbleweed.qcow2
# QCOW2=bullseye.qcow2
# QCOW2=centos9.qcow2
# QCOW2=fedora38.qcow2
# QCOW2=lunar.qcow2
QCOW2=arch.qcow2

NAME=vmname
BULK="/var/lib/cloonix/bulk"

#######################################################################
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
else
  cloonix_cli $NET rma
fi

echo waiting 2 sec
sleep 2

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------
cloonix_cli $NET add kvm ${NAME} ${PARAMS} ${QCOW2} &
#----------------------------------------------------------------------
set +e
for i in 1 2 ; do
  while ! cloonix_ssh $NET ${NAME} "echo" 2>/dev/null; do
    echo ${NAME} not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "mkdir -p ${BULK}"
cloonix_scp $NET ${BULK}/${QCOW2} ${NAME}:${BULK}/bookworm.qcow2
cloonix_scp $NET -r  ${HERE}/cloonix-bundle* ${NAME}:~
cloonix_ssh $NET ${NAME} "cd cloonix-bundle* ; ./install_cloonix"
cloonix_scp $NET ${HERE}/../quickstart/demos/ping_kvm.sh ${NAME}:~
cloonix_ssh $NET ${NAME} "./ping_kvm.sh"
#----------------------------------------------------------------------



