#!/bin/bash
set -e
HERE=`pwd`
ROOTFS=/root/centos8
MIRROR=https://mirrors.edge.kernel.org/centos/8
#MIRROR=http://127.0.0.1/centos8
#----------------------------------------------------------------------#
fct_check_uid()
{
  if [ $UID != 0 ]; then
    echo you must be root
    exit -1
  fi
}
#----------------------------------------------------------------------#
fct_check_losetup()
{
  modprobe loop
  if [[ "$(losetup -a )" != "" ]]; then
    echo
    echo
    echo losetup -a should give nothing, it gives:
    losetup -a
    echo
    exit -1
  fi
}
#----------------------------------------------------------------------#
fct_create_32G_mount_wkmntloops()
{
  truncate --size 32G ${ROOTFS}
  sgdisk --clear \
         --new 1::+1M --typecode=1:ef02 --change-name=1:'BIOS' \
         --new 2::-0 --typecode=2:8300 --change-name=2:'Linux' \
         ${ROOTFS}
  losetup /dev/loop0 ${ROOTFS}
  partprobe /dev/loop0
  mkfs.ext4 -F -L "cloonix" /dev/loop0p2
  mkdir -p /tmp/wkmntloops
  mount /dev/loop0p2 /tmp/wkmntloops
}
#----------------------------------------------------------------------#
fct_umount_wkmntloops()
{
  umount /tmp/wkmntloops
  losetup -d /dev/loop0
}
#----------------------------------------------------------------------#

#########################################################################
fct_check_uid
set +e
for i in qemu-img rpm yum blkid wget sgdisk partprobe; do
  path_bin=$(which $i)
  if [ -z $path_bin ]; then
    echo $i  DOES NOT EXIST
    exit -1
  fi
done

if [ -e $ROOTFS ]; then
  echo $ROOTFS already exists
  exit 1
fi

fct_check_losetup
set -e
fct_create_32G_mount_wkmntloops $ROOTFS
#-----------------------------------------------------------------------#
set +e
for i in ${MIRROR}/BaseOS/x86_64/os ${MIRROR}/AppStream/x86_64/os;
do
  wget --no-check-certificate --delete-after ${i} 1>/dev/null 2>&1
  OK=$?
  if [ "$OK" != "0" ]; then
    echo ERROR wget ${i}
    exit 1
  fi
  echo
  echo REPO OK ${i}
  echo
done
set -e
#-----------------------------------------------------------------------#
cd /tmp/wkmntloops
mkdir -p /tmp/wkmntloops/etc/yum.repos.d

cat > /tmp/wkmntloops/etc/yum.repos.d/CentOS-Base.repo << EOF
[base]
name=CentOS-8 - Base
baseurl=${MIRROR}/BaseOS/x86_64/os
gpgcheck=0
EOF

cat > /tmp/wkmntloops/etc/yum.repos.d/CentOS-AppStream.repo << EOF
[AppStream]
name=CentOS-8 - AppStream
baseurl=${MIRROR}/AppStream/x86_64/os
gpgcheck=0
EOF

cat > /tmp/wkmntloops/etc/yum.repos.d/CentOS-Extras.repo << EOF
[extras]
name=CentOS-8 - Extras
baseurl=${MIRROR}/extras/x86_64/os
gpgcheck=0
EOF


yum --releasever=8 --installroot=/tmp/wkmntloops -y install dnf
#-----------------------------------------------------------------------#
if [ -e /etc/resolv.conf ]; then
  cp -f /etc/resolv.conf "/tmp/wkmntloops/etc"
fi
#-----------------------------------------------------------------------#
list_pkt="kernel grub2-pc passwd openssh-clients "
list_pkt+="xorg-x11-xauth iputils dhclient sudo kbd "
list_pkt+="vim net-tools iw dracut"
for d in dev sys proc; do mount --bind /$d /tmp/wkmntloops/$d; done
sleep 5
echo dnf --releasever=8 clean packages
chroot /tmp/wkmntloops/ dnf --releasever=8 clean packages
echo dnf --releasever=8 -y update
chroot /tmp/wkmntloops/ dnf --releasever=8 -y update
echo dnf --releasever=8 -y install
chroot /tmp/wkmntloops/ dnf --releasever=8 -y install $list_pkt
chroot /tmp/wkmntloops/ grub2-install --no-floppy --modules=part_gpt --target=i386-pc /dev/loop0

KERN="noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
printf "\nGRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "$KERN" \
        > /tmp/wkmntloops/etc/default/grub

for i in $(ls /tmp/wkmntloops/boot | grep vmlinuz);do vmlinuz=$i;done
version=${vmlinuz#*vmlinuz-}
echo $version
list="virtio_scsi virtio_blk virtio_console" 
chroot /tmp/wkmntloops/ dracut --add-drivers "$list" --force /boot/initramfs-${version}.img $version 

chroot /tmp/wkmntloops/ grub2-mkconfig -o /boot/grub2/grub.cfg
uuid=$(blkid | grep cloonix)
uuid=${uuid#*UUID=\"}
uuid=${uuid%%\"*}
echo $uuid
sync /dev/loop0
umount /tmp/wkmntloops/{dev,proc,sys}
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/fstab  << EOF
UUID=$uuid  /            ext4     defaults                      0 0 
proc        /proc        proc     nosuid,noexec,nodev           0 0
sysfs       /sys         sysfs    nosuid,noexec,nodev           0 0
devpts      /dev/pts     devpts   nosuid,noexec,gid=5,mode=620  0 0
tmpfs       /run         tmpfs    defaults                      0 0
devtmpfs    /dev         devtmpfs mode=0755,nosuid              0 0
EOF
#-----------------------------------------------------------------------#
chroot /tmp/wkmntloops/ passwd root <<EOF
root
root
EOF
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/systemd/system/serial-getty@hvc0.service << EOF
[Service]
ExecStart=-/sbin/agetty -a root hvc0
Type=idle
Restart=always
RestartSec=0
EOF
#-----------------------------------------------------------------------#
cd /tmp/wkmntloops/etc/systemd/system/getty.target.wants
ln -s /etc/systemd/system/serial-getty@hvc0.service serial-getty@hvc0.service
cd ${HERE}
#-----------------------------------------------------------------------#
sed -i s"/127.0.0.1/172.17.0.2/" /tmp/wkmntloops/etc/yum.repos.d/CentOS-Base.repo
sed -i s"/127.0.0.1/172.17.0.2/" /tmp/wkmntloops/etc/yum.repos.d/CentOS-AppStream.repo
sed -i s"/127.0.0.1/172.17.0.2/" /tmp/wkmntloops/etc/yum.repos.d/CentOS-Extras.repo
#-----------------------------------------------------------------------#
fct_umount_wkmntloops
#-----------------------------------------------------------------------#
qemu-img convert -O qcow2 $ROOTFS ${ROOTFS}.qcow2
rm -f $ROOTFS
#-----------------------------------------------------------------------#

