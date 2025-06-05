#!/bin/bash
#----------------------------------------------------------------------------#
NET="nemo"
NAME="docker"
DEFVIM="/usr/share/vim/vim91/defaults.vim"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/trixie"
TRIXIE0="/var/lib/cloonix/bulk/trixie0"
TRIXIE="/var/lib/cloonix/bulk/trixie_docker"
#----------------------------------------------------------------------------#
if [ ! -e ${TRIXIE0} ]; then
  echo ${TRIXIE0} does not exist!
  exit 1
fi
#----------------------------------------------------------------------------#
if [ -e ${TRIXIE} ]; then
  echo ${TRIXIE} already exists, please erase
  exit 1
fi
#-----------------------------------------------------------------------#
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#----------------------------------------------------------------------#
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ ! -z "${is_started}" ]; then
  echo cloonix $NET working! Please kill it with:
  echo cloonix_cli $NET kil
  exit 1
fi
#----------------------------------------------------------------------#
cp -rf ${TRIXIE0} ${TRIXIE}
sync
sleep 5
sync
#----------------------------------------------------------------------#
cloonix_net ${NET}
cloonix_gui ${NET}
cloonix_cli ${NET} add cvm ${NAME} eth=s trixie_docker --persistent
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan ${NAME} 0 lan1
cloonix_cli ${NET} add lan nat 0 lan1
#----------------------------------------------------------------------------#
set +e
while ! cloonix_ssh ${NET} ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 3 sec
  sleep 3
done
set -e
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "ip addr add dev eth0 172.17.0.12/24"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes update"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes install \
                      openssh-client vim bash-completion \
                      net-tools systemd x11-apps strace rsyslog \
                      lsof xauth file docker-compose docker.io \
                      docker-cli curl"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "sed -i s'/filetype plugin/\"filetype plugin/' $DEFVIM"
cloonix_ssh ${NET} ${NAME} "sed -i s'/set mouse/\"set mouse/' $DEFVIM"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "systemctl enable rsyslog.service"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "sync"
sleep 3
set +e
cloonix_ssh ${NET} ${NAME} "poweroff"
sleep 5
cloonix_cli ${NET} kil
echo END
#----------------------------------------------------------------------------#

