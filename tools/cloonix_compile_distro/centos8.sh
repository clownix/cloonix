#!/bin/bash
HERE=`pwd`
NET=nemo
NAME=tool
QCOW2=centos8.qcow2
CLOONIX_CENTOS8_REPO=https://mirrors.edge.kernel.org/centos/8
#CLOONIX_CENTOS8_REPO=http://172.17.0.2/centos8
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
cloonix_cli $NET add kvm $NAME ram=8000 cpu=4 dpdk=0 sock=2 hwsim=0 $QCOW2
#----------------------------------------------------------------------
cloonix_cli ${NET} add nat nat

#######################################################################
set +e
while ! cloonix_ssh $NET ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 5 sec
  sleep 5
done
set -e
#----------------------------------------------------------------------
cloonix_cli ${NET} add lan ${NAME} 0 lan1
cloonix_cli ${NET} add lan nat 0 lan1
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "mkdir -p /root/cloonix_data/bulk"
#----------------------------------------------------------------------
cd ${HERE}/../..
./allclean
cd ${HERE}/../../..
mkdir -p /tmp/${NET}
tar zcvf /tmp/${NET}/sources.tar.gz ./sources
cloonix_scp $NET /tmp/${NET}/sources.tar.gz ${NAME}:/root
rm -rf /tmp/${NET}
cd ${HERE}
#----------------------------------------------------------------------
cloonix_scp $NET ${BULK}/buster.qcow2 ${NAME}:/root/cloonix_data/bulk
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "dhclient"
#----------------------------------------------------------------------
cloonix_ssh ${NET} ${NAME} "cat > /etc/yum.repos.d/CentOS-Base.repo << EOF
[base]
name=CentOS-8 - Base
baseurl=${CLOONIX_CENTOS8_REPO}/BaseOS/x86_64/os
gpgcheck=0
EOF"
cloonix_ssh ${NET} ${NAME} "cat > /etc/yum.repos.d/CentOS-AppStream.repo << EOF
[AppStream]
name=CentOS-8 - AppStream
baseurl=${CLOONIX_CENTOS8_REPO}/AppStream/x86_64/os
gpgcheck=0
EOF"
cloonix_ssh ${NET} ${NAME} "cat > /etc/yum.repos.d/CentOS-Extras.repo << EOF
[extras]
name=CentOS-8 - Extras
baseurl=${CLOONIX_CENTOS8_REPO}/extras/x86_64/os
gpgcheck=0
EOF"
cloonix_ssh ${NET} ${NAME} "cat > /etc/yum.repos.d/CentOS-PowerTools.repo << EOF
[PowerTools]
name=CentOS-8 - PowerTools
baseurl=${CLOONIX_CENTOS8_REPO}/PowerTools/x86_64/os
gpgcheck=0
EOF"
#----------------------------------------------------------------------
cloonix_ssh ${NET} ${NAME} "cat > /etc/resolv.conf << EOF
nameserver 172.17.0.3
EOF"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "dnf update -y; dnf -y install tar"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "tar xvf sources.tar.gz"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "rm sources.tar.gz"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd sources ; ./install_depends"
#----------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "cd sources ; ./doitall"
#----------------------------------------------------------------------





