#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
#----------------------------------------------------------------------#
if [ ${#} = 1 ]; then
  NAME=${1}
  case "${NAME}" in
    "debian")
      ISO="${ISOPATH}/debian-12.9.0-amd64-DVD-1.iso" ;;
    "fedora")
      ISO="${ISOPATH}/Fedora-Workstation-Live-x86_64-41-1.4.iso" ;;
    "ubuntu")
      ISO="${ISOPATH}/ubuntu-24.04.2-desktop-amd64.iso" ;;
    "centos")
      ISO="${ISOPATH}/CentOS-Stream-10-latest-x86_64-dvd1.iso" ;;
    "linuxmint")
      ISO="${ISOPATH}/linuxmint-22.1-cinnamon-64bit.iso" ;;
    "mxlinux")
      ISO="${ISOPATH}/MX-23.5_KDE_x64.iso" ;;
    *)
      echo debian fedora ubuntu centos archlinux linuxmint mxlinux
      exit
      ;;
    esac
else
  echo debian fedora ubuntu centos archlinux linuxmint mxlinux
  exit
fi
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
cloonix_cli $NET add kvm ${NAME} ram=3000 cpu=2 eth=s ${NAME}_iso.qcow2 &
#----------------------------------------------------------------------------
while ! cloonix_ssh $NET ${NAME} "echo"; do
  echo $NAME not ready, waiting 3 sec
  sleep 3
done
#----------------------------------------------------------------------------
cloonix_scp $NET /home/perrier/cloonix_extractor.sh ${NAME}:/home/user
cloonix_ssh $NET ${NAME} "chown user:user /home/user/cloonix_extractor.sh"
cloonix_ssh $NET ${NAME} "su -c 'chmod +x /home/user/cloonix_extractor.sh' user"
#----------------------------------------------------------------------------

