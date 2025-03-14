#!/bin/bash
#------------------------------------------------------------------------
HERE=`pwd`
ISOPATH="/media/perrier/T7Shield/from_iso"
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
QCOW2="/var/lib/cloonix/bulk/${NAME}_iso.qcow2"
QEMU_BIN="/usr/libexec/cloonix/server/cloonix-qemu-system"
QEMU_DIR="/usr/libexec/cloonix/server/qemu"
#------------------------------------------------------------------------
if [ ! -e ${ISO} ]; then
  echo missing ${ISO}
  exit 1
fi
if [ ! -e ${QEMU_BIN} ]; then
  echo ${QEMU_BIN} does not exist
  echo Install cloonix
  exit 1
fi
#------------------------------------------------------------------------
rm -f ${QCOW2}
qemu-img create -f qcow2 ${QCOW2} 60G
#------------------------------------------------------------------------
sync
sleep 1
#------------------------------------------------------------------------
${QEMU_BIN} \
     -L ${QEMU_DIR} -enable-kvm -m 6000 \
     -cpu host,+vmx -smp 4 -no-reboot \
     -vga virtio \
     -nodefaults \
     -drive file=${QCOW2},id=hd,media=disk,cache=none,if=none \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -boot d \
     -cdrom ${ISO}
#------------------------------------------------------------------------
echo
echo
echo DONE ${QCOW2}



