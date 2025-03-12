#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET="nemo"
#----------------------------------------------------------------------#

#----------------------------------------------------------------------#
VMNAME="fed"
QCOW2="fedora_iso.qcow2"
#----------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
#----------------------------------------------------------------------#
PARAMS="ram=3000 cpu=2 eth=s"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent --no_qemu_ga
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
#-----------------------------------------------------------------------#
cloonix_gui ${NET}
#-----------------------------------------------------------------------#
# In spice desktop, set
# SELINUX=disabled in file: /etc/sysconfig/selinux
#-----------------------------------------------------------------------#

