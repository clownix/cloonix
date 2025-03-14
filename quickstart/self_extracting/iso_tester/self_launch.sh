#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
SELF="cloonix_extractor.sh"
BULK="/var/lib/cloonix/bulk"
#----------------------------------------------------------------------#
#----------------------------------------------------------------------#
if [ ${#} = 1 ]; then
  NAME=${1}
  case "${NAME}" in
    "debian")
       ETH0="enp0s5"
       ;;
    "fedora")
       ETH0="enp0s5"
       ;;
    "ubuntu")
       ETH0="enp0s5"
       ;;
    "centos")
       ETH0="enp0s5"
       ;;
    "linuxmint")
       ETH0="enp0s5"
       ;;
    "mxlinux")
       ETH0="eth0"
       ;;
    *)
      echo debian fedora ubuntu centos linuxmint mxlinux
      exit
      ;;
    esac
else
  echo debian fedora ubuntu centos linuxmint mxlinux
  exit
fi
#----------------------------------------------------------------------#
if [ ! -e "${HOME}/${SELF}" ]; then
  echo NOT FOUND:
  echo "${HOME}/${SELF}"
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
cloonix_cli $NET add kvm ${NAME} ram=8000 cpu=4 eth=s ${NAME}_iso.qcow2 &
cloonix_cli $NET add tap nemotap1
cloonix_cli $NET add lan nemotap1 0 lan
cloonix_cli $NET add lan ${NAME} 0 lan
#----------------------------------------------------------------------------
while ! cloonix_ssh $NET ${NAME} "echo"; do
  echo $NAME not ready, waiting 3 sec
  sleep 3
done
#----------------------------------------------------------------------------
cloonix_ssh $NET ${NAME} "mkdir -p /var/lib/cloonix/bulk"
for i in "zipfrr.zip" "bookworm.qcow2"; do
  cloonix_scp $NET ${BULK}/${i} ${NAME}:/${BULK}
done
cloonix_scp $NET ${HOME}/${SELF} ${NAME}:/root
cloonix_ssh $NET ${NAME} "./${SELF}"
sudo ifconfig nemotap1 1.1.1.1/24 up
cloonix_ssh $NET ${NAME} "systemctl stop NetworkManager.service"
cloonix_ssh $NET ${NAME} "ip addr add dev $ETH0 1.1.1.2/24" 
cloonix_ssh $NET ${NAME} "./nemocmd start"
cloonix_ssh $NET ${NAME} "./nemocmd canvas"
cloonix_ssh $NET ${NAME} "./nemocmd web_on"
cloonix_ssh $NET ${NAME} "./nemocmd demo_ping"
#----------------------------------------------------------------------------

