#!/bin/bash
HERE=`pwd`
NET=fido
NAME=cloon
CLOONIX=/home/cloonix
DISTRO=$1
case "${DISTRO}" in
  "buster")
    ;;
  "bullseye")
    ;;
  *)
    echo ERROR FIRST PARAM: ${DISTRO} Choice: buster bullseye
    exit 1
esac
QCOW2=${DISTRO}.qcow2
CLOONIX_BUSTER_REPO="http://deb.debian.org/debian"
#CLOONIX_BUSTER_REPO="http://172.17.0.2/${DISTRO}"

#######################################################################
CLOONIX_CONFIG=/usr/local/bin/cloonix/cloonix_config
CLOONIX_BULK=$(cat $CLOONIX_CONFIG |grep CLOONIX_BULK | awk -F = "{print \$2}")
BULK=$(eval echo $CLOONIX_BULK)
if [ ! -e ${BULK}/${DISTRO}.qcow2 ]; then
  echo
  echo Need a small virtual qcow2 to test cloonix inside vm.
  echo Missing:
  echo ${BULK}/${DISTRO}.qcow2
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
cloonix_cli ${NET} add nat nat
#----------------------------------------------------------------------
cloonix_cli $NET add kvm $NAME ram=8000 cpu=4 dpdk=0 sock=1 hwsim=0 $QCOW2 &
#----------------------------------------------------------------------
sleep 3
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
cloonix_ssh $NET ${NAME} "mkdir -p ${CLOONIX}/cloonix_data/bulk"
#----------------------------------------------------------------------
cd ${HERE}/../..
./allclean
cd ${HERE}/../../..
mkdir -p /tmp/${NET}
tar zcvf /tmp/${NET}/sources.tar.gz ./sources
cloonix_scp $NET /tmp/${NET}/sources.tar.gz ${NAME}:${CLOONIX}
rm -rf /tmp/${NET}
cd ${HERE}
#----------------------------------------------------------------------
cloonix_scp $NET ${BULK}/${DISTRO}.qcow2 ${NAME}:${CLOONIX}/cloonix_data/bulk
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}; tar xvf sources.tar.gz"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "rm ${CLOONIX}/sources.tar.gz"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "echo \"wireshark-common "\
                         "wireshark-common/install-setuid boolean true\" "\
                         " > /tmp/custom_preseed.txt"
cloonix_ssh $NET ${NAME} "debconf-set-selections /tmp/custom_preseed.txt"
cloonix_ssh $NET ${NAME} "dhclient"
#----------------------------------------------------------------------
cloonix_ssh ${NET} ${NAME} "cat > /etc/apt/sources.list << EOF
deb ${CLOONIX_BUSTER_REPO} ${DISTRO} main contrib non-free
EOF"
#----------------------------------------------------------------------
cloonix_ssh ${NET} ${NAME} "cat > /etc/resolv.conf << EOF
nameserver 172.17.0.3
EOF"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}/sources ; ./install_depends"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd ${CLOONIX}/sources ; ./doitall"
cloonix_ssh $NET ${NAME} "sed -i s%\\\${HOME}%/home/cloonix% "\
                         "/usr/local/bin/cloonix/cloonix_config"
#----------------------------------------------------------------------





