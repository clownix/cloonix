#!/bin/bash
#-----------------------------------------------------------------------#
HERE=`pwd`
DISTRO="openwrt"
OPENWRT="https://downloads.openwrt.org/releases/24.10.0/targets/x86/64"
ROOTFS=/root/${DISTRO}
OFFSET=17301504
OFFSET_BOOT=262144
OPENWRT_IMG="openwrt-24.10.0-x86-64-generic-ext4-combined.img"

for i in qemu-img gunzip wget; do
  path_bin=$(which $i)
  if [ -z $path_bin ]; then
    echo $i  DOES NOT EXIST
    exit -1
  fi
done

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi
modprobe loop
if [[ "$(losetup -a )" != "" ]]; then
  echo
  echo
  echo losetup -a should give nothing, it gives:
  losetup -a
  echo
  exit -1
fi
#---------------------------------------------------------------------------#
  mkdir -p /tmp/wkmntloops
  wget --no-check-certificate ${OPENWRT}/${OPENWRT_IMG}.gz
  gunzip ${OPENWRT_IMG}.gz
  qemu-img convert -O raw ${OPENWRT_IMG} $ROOTFS
  rm ${OPENWRT_IMG}
  losetup /dev/loop0 $ROOTFS
  losetup -o $OFFSET /dev/loop1 /dev/loop0
  mount /dev/loop1 /tmp/wkmntloops
#--------------------------------------------------------------------#
rm -f /tmp/wkmntloops/etc/resolv.conf
cp -f /etc/resolv.conf "/tmp/wkmntloops/etc"
#-----------------------------------------------------------------------#
  cat > /tmp/wkmntloops/chroot_cmds  << "EOF"
#!/bin/ash
passwd << "INEOF"
rootroot
rootroot
INEOF
mkdir /var/lock

opkg update --force-checksum --no-check-certificate
sleep 2

opkg update --force-checksum --no-check-certificate
sleep 2

opkg install qemu-ga --force-checksum --no-check-certificate
sleep 2

opkg update --force-checksum --no-check-certificate
sleep 2

opkg install qemu-ga --force-checksum --no-check-certificate
sleep 2
EOF

  chmod +x /tmp/wkmntloops/chroot_cmds
  chroot /tmp/wkmntloops /chroot_cmds

  rm -f /tmp/wkmntloops/chroot_cmds
  umount /tmp/wkmntloops
  losetup -d /dev/loop1
  losetup -d /dev/loop0
  cd /root
  qemu-img convert -O qcow2 openwrt /var/lib/cloonix/bulk/openwrt.qcow2
  rm -f openwrt
#---------------------------------------------------------------------#




