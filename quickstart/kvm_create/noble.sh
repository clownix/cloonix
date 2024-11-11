#!/bin/bash
HERE=`pwd`
#----------------------------------------------------------------------#
# Ubuntu 24.04
DISTRO=noble
ROOTFS=/var/lib/cloonix/bulk/${DISTRO}
UBUNTU_REPO="http://fr.archive.ubuntu.com/ubuntu"
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
for i in qemu-img debootstrap blkid wget sgdisk partprobe; do
  path_bin=$(which $i)
  if [ -z $path_bin ]; then
    echo $i  DOES NOT EXIST
    exit -1
  fi
done
#qemu-img: qemu-utils
#debootstrap: debootstrap
#blkid: util-linux
#sgdisk: gdisk
#partprobe: parted

if [ -e $ROOTFS ]; then
  echo $ROOTFS already exists
  exit 1
fi

fct_check_losetup
set -e
fct_create_32G_mount_wkmntloops $ROOTFS
#-----------------------------------------------------------------------#
if [ ! -e /usr/share/debootstrap/scripts/${DISTRO} ]; then
  cd /usr/share/debootstrap/scripts
  ln -s gutsy ${DISTRO}
  cd $HERE
fi
#-----------------------------------------------------------------------#

list_pkt="linux-generic,grub-pc,openssh-client,bash-completion,"
list_pkt+="xauth,sudo,kbd,vim,net-tools,initramfs-tools,"
list_pkt+="qemu-guest-agent"

debootstrap --no-check-certificate --no-check-gpg --arch amd64 \
            --components=kernel,multiverse,universe,main \
            --include=$list_pkt \
            ${DISTRO} \
            /tmp/wkmntloops ${UBUNTU_REPO}
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/apt/sources.list << EOF
deb ${UBUNTU_REPO} ${DISTRO} main universe
deb ${UBUNTU_REPO} ${DISTRO}-security main universe 
deb ${UBUNTU_REPO} ${DISTRO}-updates main universe 
EOF
#-----------------------------------------------------------------------#
for d in dev sys proc; do mount --bind /$d /tmp/wkmntloops/$d; done
chroot /tmp/wkmntloops/ grub-install --no-floppy --modules=part_gpt --target=i386-pc /dev/loop0
KERN="rw noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
printf "\nGRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "$KERN" \
        >> /tmp/wkmntloops/etc/default/grub
chroot /tmp/wkmntloops/ update-grub
echo "source /etc/bash_completion" >> /tmp/wkmntloops/root/.bashrc
sync /dev/loop0
umount /tmp/wkmntloops/{dev,proc,sys}
#-----------------------------------------------------------------------#
uuid=$(blkid | grep loop0p2 | grep cloonix)
uuid=${uuid#*UUID=\"}
uuid=${uuid%%\"*}
echo $uuid

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
fct_umount_wkmntloops
#-----------------------------------------------------------------------#
qemu-img convert -O qcow2 $ROOTFS ${ROOTFS}.qcow2
rm -f $ROOTFS
#-----------------------------------------------------------------------#

