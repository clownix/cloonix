#!/bin/bash
NET=nemo
NAME=cloon
CLOONIX=/home/cloonix
TARGET=$1
case "${TARGET}" in
  "xubuntu_fr")
  ;;
  "stretch_fr")
  ;;
  "buster_fr")
  ;;
  "ubuntu-18.04_fr")
  ;;
  "ubuntu-19.04_fr")
  ;;
  "redhat8_fr")
  ;;
  "centos8_fr")
  ;;
  "fedora30_fr")
  ;;
  "opensuse15_fr")
  ;;
  "opensuseweed_fr")
  ;;
  *)
  echo Missing param:
  echo redhat8_fr centos8_fr fedora30_fr
  echo ubuntu-18.04_fr ubuntu-19.04_fr stretch_fr buster_fr
  echo opensuse15_fr opensuseweed_fr
  exit 1
  ;;
esac
QCOW2=${TARGET}.qcow2
echo CHOICE: $QCOW2

#######################################################################
CLOONIX_CONFIG=/usr/local/bin/cloonix/cloonix_config
CLOONIX_BULK=$(cat $CLOONIX_CONFIG |grep CLOONIX_BULK | awk -F = "{print \$2}")
BULK=$(eval echo $CLOONIX_BULK)
if [ ! -e ${BULK}/buster.qcow2 ]; then
  echo
  echo Need a small virtual qcow2 to test cloonix inside vm.
  echo Missing:
  echo ${BULK}/buster.qcow2
  echo
  exit 1
fi


#######################################################################
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
  echo waiting 2 sec
  sleep 2
else
  cloonix_cli $NET rma
  echo waiting 10 sec
  sleep 10
fi
#----------------------------------------------------------------------

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------

#######################################################################
cloonix_cli ${NET} add nat nat
#----------------------------------------------------------------------
cloonix_cli $NET add kvm $NAME ram=12000 cpu=4 dpdk=0 sock=1 hwsim=0 $QCOW2 &
#----------------------------------------------------------------------
sleep 3
#######################################################################
cloonix_cli ${NET} add lan ${NAME} 0 lan1
cloonix_cli ${NET} add lan nat 0 lan1
#----------------------------------------------------------------------

#######################################################################
set +e
while ! cloonix_ssh $NET ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------

#######################################################################
tar zcvf /tmp/sources.tar.gz ../../sources
cloonix_scp $NET /tmp/sources.tar.gz ${NAME}:${CLOONIX}
rm /tmp/sources.tar.gz
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "mkdir -p ${CLOONIX}/cloonix_data/bulk"
cloonix_scp $NET ${BULK}/buster.qcow2 ${NAME}:${CLOONIX}/cloonix_data/bulk
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}; tar xvf sources.tar.gz"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "rm ${CLOONIX}/sources.tar.gz"
#----------------------------------------------------------------------
case "${TARGET}" in
  "buster_fr" | "stretch_fr" | "ubuntu-18.04_fr" | "ubuntu-19.04_fr")
    cloonix_ssh $NET ${NAME} "echo \"wireshark-common "\
                "wireshark-common/install-setuid boolean true\" "\
                " > /tmp/custom_preseed.txt"
    cloonix_ssh $NET ${NAME} "debconf-set-selections /tmp/custom_preseed.txt"
    cloonix_ssh $NET ${NAME} "dhclient"
  ;;
  "redhat8_fr" | "centos8_fr" | "fedora30_fr")
    cloonix_ssh $NET ${NAME} "dhclient"
  ;;
  "opensuse15_fr" | "opensuseweed_fr")
    cloonix_ssh $NET ${NAME} "systemctl stop packagekit.service"
    cloonix_ssh $NET ${NAME} "systemctl disable packagekit.service"
    sleep 10
  ;;
  *)
  echo ERROR
  exit 1
  ;;
esac
#----------------------------------------------------------------------
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}/sources ; ./install_depends"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}/sources ; ./doitall"
cloonix_ssh $NET ${NAME} "sed -i s%\\\${HOME}%/home/cloonix% "\
                         "/usr/local/bin/cloonix/cloonix_config"
#----------------------------------------------------------------------





