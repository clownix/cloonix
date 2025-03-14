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
      echo debian fedora ubuntu centos linuxmint mxlinux
      exit
      ;;
    esac
else
  echo debian fedora ubuntu centos linuxmint mxlinux
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
cloonix_cli $NET add kvm ${NAME} ram=3000 cpu=2 eth=s ${NAME}_iso.qcow2 --persistent --no_qemu_ga 
#----------------------------------------------------------------------------
cloonix_cli $NET add nat nat
cloonix_cli $NET add lan ${NAME} 0 lan
cloonix_cli $NET add lan nat 0 lan
#----------------------------------------------------------------------------

