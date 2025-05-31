#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
DISTRO="alpine"
STORE="/media/perrier/T7Shield2/alpine"
NAMEROOT="alpine-minirootfs-3.21.3-x86_64"
ROOTFS="/var/lib/cloonix/bulk/${DISTRO}"
#----------------------------------------------------------------------#
if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi
#----------------------------------------------------------------------#
if [ ! -e ${STORE}/${NAMEROOT}.tar.gz ]; then
  echo MISSING ${STORE}/${NAMEROOT}.tar.gz
  exit 1
fi
#----------------------------------------------------------------------#
for i in "qemu-img" "sgdisk" "partprobe"; do
  path_bin=$(which ${i})
  if [ -z $path_bin ]; then
    echo ${i}  DOES NOT EXIST
    exit -1
  fi
done
#----------------------------------------------------------------------#
if [ -e $ROOTFS ]; then
  echo $ROOTFS already exists
  exit 1
fi
#----------------------------------------------------------------------#
modprobe loop
if [ ! -z "$(losetup -a)" ]; then
  echo losetup -a should give nothing, it gives:
  losetup -a
  exit -1
fi
#----------------------------------------------------------------------#
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
#----------------------------------------------------------------------#
cd /tmp/wkmntloops
tar xvf ${STORE}/${NAMEROOT}.tar.gz
cd ${HERE}
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/apk/repositories << EOF
http://127.0.0.1/alpine/main
http://127.0.0.1/alpine/community
EOF
#-----------------------------------------------------------------------#
for d in dev sys proc; do mount --bind /$d /tmp/wkmntloops/$d; done
chroot /tmp/wkmntloops apk update --allow-untrusted
#-----------------------------------------------------------------------#
chroot /tmp/wkmntloops apk --allow-untrusted add syslinux
chroot /tmp/wkmntloops apk --allow-untrusted add grub
chroot /tmp/wkmntloops apk --allow-untrusted add grub-bios
chroot /tmp/wkmntloops grub-install --no-floppy --module=part_gpt --target=i386-pc /dev/loop0
KERN="rw noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
printf "\nGRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "$KERN" \
        >> /tmp/wkmntloops/etc/default/grub
chroot /tmp/wkmntloops/ update-grub
#-----------------------------------------------------------------------#
mkdir -p /tmp/wkmntloops/etc/mkinitfs/modules.d
cat > /tmp/wkmntloops/etc/mkinitfs/modules.d/scsi  << EOF
kernel/drivers/block/virtio*
kernel/drivers/virtio/virtio*
kernel/drivers/scsi/virtio*
kernel/drivers/net/virtio*
kernel/drivers/char/virtio*
EOF
cat >> /tmp/wkmntloops/etc/modules << EOF
fuse
vhost-net
EOF

for i in "musl" \
         "musl-utils" \
         "musl-locales" \
         "tzdata" \
         "linux-virt" \
         "mkinitfs" \
         "qemu-guest-agent" \
         "openrc" \
         "alpine-base" \
         "kbd" \
         "xauth" \
         "kbd-bkeymaps" \
         "dhcpcd"; do
  chroot /tmp/wkmntloops apk --allow-untrusted --no-cache --update add ${i}
done
#-----------------------------------------------------------------------#
for i in "mount-ro" "killprocs" "savecache"; do
  chroot /tmp/wkmntloops rc-update add ${i} shutdown
done
for i in "hwclock" "modules" "sysctl" "hostname" "bootmisc" "syslog"; do
  chroot /tmp/wkmntloops rc-update add ${i} boot
done 
for i in "devfs" "dmesg" "mdev" "hwdrivers" "qemu-guest-agent" "cgroups"; do
  chroot /tmp/wkmntloops rc-update add ${i} sysinit
done 
chroot /tmp/wkmntloops setup-keymap fr fr
chroot /tmp/wkmntloops setup-hostname alpine
#-----------------------------------------------------------------------#
sync /dev/loop0
umount /tmp/wkmntloops/{dev,proc,sys}
#-----------------------------------------------------------------------#
chroot /tmp/wkmntloops/ passwd root <<EOF
root
root
EOF
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/apk/repositories << EOF
http://172.17.0.2/alpine/main
http://172.17.0.2/alpine/community
EOF
#-----------------------------------------------------------------------#
umount /tmp/wkmntloops
losetup -d /dev/loop0
#-----------------------------------------------------------------------#
qemu-img convert -O qcow2 ${ROOTFS} ${ROOTFS}.qcow2
rm -f ${ROOTFS}
#-----------------------------------------------------------------------#

