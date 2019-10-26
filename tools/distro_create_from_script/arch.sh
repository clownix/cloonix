#!/bin/bash
HERE=`pwd`
ROOTFS=/root/arch
BOOTPATH="https://mirrors.edge.kernel.org/archlinux/iso/2019.10.01"
BOOTNAME="archlinux-bootstrap-2019.10.01-x86_64.tar.gz"
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
for i in qemu-img blkid wget sgdisk partprobe; do
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
#-----------------------------------------------------------------------#
cd /root
if [ ! -e ${BOOTNAME} ]; then
  wget --no-check-certificate ${BOOTPATH}/${BOOTNAME}
fi
if [ ! -e ${BOOTNAME} ]; then
  echo needs the bootstrap for arch
  exit 1
fi
tar xvf ${BOOTNAME}
if [ ! -d root.x86_64 ]; then
  echo needs the bootstrap for arch
  exit 1
fi
cd ${HERE}
#-----------------------------------------------------------------------#
set -e
fct_create_32G_mount_wkmntloops $ROOTFS
#-----------------------------------------------------------------------#
mv /root/root.x86_64/* /tmp/wkmntloops
rmdir /root/root.x86_64
#-----------------------------------------------------------------------#
fct_kill_wkmntloop_pids()
{
  set +e
  pid=$(lsof | grep /tmp/wkmntloops | awk "{print \$2}")
  if [ "$pid" != "" ]; then
    uniq_pid=$(printf '%s\n' $pid | sort -u)
    for i in $uniq_pid; do
      if [ "$i" != "$$" ]; then
        kill -9 $i 1>/dev/null 2>&1
      else
        echo Lock by own process happens if you are in dir wkmntloop/... 
      fi
    done
  fi
  set -e
  sleep 3
}
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/etc/pacman.d/mirrorlist  << "EOF"
Server = http://mirrors.acm.wpi.edu/archlinux/$repo/os/$arch
Server = http://mirrors.advancedhosters.com/archlinux/$repo/os/$arch
Server = http://mirrors.aggregate.org/archlinux/$repo/os/$arch
Server = http://ca.us.mirror.archlinux-br.org/$repo/os/$arch
Server = http://il.us.mirror.archlinux-br.org/$repo/os/$arch
EOF
#-----------------------------------------------------------------------#
cat > /tmp/wkmntloops/root/chroot_cmds  << "EOF"
#!/bin/bash
pacman-key --init
pacman-key --populate archlinux
pacman -Syyu --noconfirm
pacman -S --noconfirm dhclient openssh xorg-xauth \
                      grep sed systemd linux-lts \
                      net-tools grub iw vim dracut
grub-install --no-floppy --modules=part_gpt --target=i386-pc /dev/loop0
EOF
chmod +x /tmp/wkmntloops/root/chroot_cmds
#-----------------------------------------------------------------------#
cd ${HERE}
cp /etc/resolv.conf /tmp/wkmntloops/etc
for d in dev sys proc; do mount --bind /$d /tmp/wkmntloops/$d; done
sleep 5
chroot /tmp/wkmntloops /root/chroot_cmds

for i in $(ls /tmp/wkmntloops/boot | grep vmlinuz);do vmlinuz=$i;done
version_suffix=${vmlinuz#*vmlinuz-}

version_kernel=$(find /tmp/wkmntloops/lib/modules |grep virtio_blk.ko)
version_kernel=${version_kernel##*modules/}
version_kernel=${version_kernel%%/kernel*}

echo $version_suffix
echo $version_kernel

list="virtio_scsi virtio_blk virtio_console"
chroot /tmp/wkmntloops/ dracut --add-drivers "$list" --force /boot/initramfs-${version_suffix}.img $version_kernel

KERN="noquiet console=ttyS0 console=tty1 earlyprintk=serial net.ifnames=0"
printf "\nGRUB_CMDLINE_LINUX_DEFAULT=\"%s\"\n" "$KERN" \
        >> /tmp/wkmntloops/etc/default/grub

chroot /tmp/wkmntloops/ grub-mkconfig -o /boot/grub/grub.cfg
fct_kill_wkmntloop_pids
sync /dev/loop0
umount /tmp/wkmntloops/{dev,proc,sys}
#-----------------------------------------------------------------------#
uuid=$(blkid | grep cloonix)
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
cat > /tmp/wkmntloops/etc/systemd/system/serial-getty@hvc0.service << EOF
[Service]
ExecStart=-/sbin/agetty -a root hvc0
Type=idle
Restart=always
RestartSec=0
EOF
cd /tmp/wkmntloops/etc/systemd/system/getty.target.wants
ln -s /etc/systemd/system/serial-getty@hvc0.service serial-getty@hvc0.service 
cd ${HERE}
#-----------------------------------------------------------------------#
fct_umount_wkmntloops
#-----------------------------------------------------------------------#
qemu-img convert -O qcow2 $ROOTFS ${ROOTFS}.qcow2
rm -f $ROOTFS
#-----------------------------------------------------------------------#

