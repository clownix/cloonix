#!/bin/bash
#----------------------------------------------------------------------#
BULK="/home/perrier/cloonix_data/bulk"
#----------------------------------------------------------------------#
IMAGE=${BULK}/bookworm.img
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
  if [[ "$(losetup -a |grep loop10)" != "" ]]; then
    echo
    echo
    echo losetup -a |grep loop10 should give nothing, it gives:
    losetup -a grep loop10
    echo
    exit -1
  fi
}
#----------------------------------------------------------------------#

#########################################################################
fct_check_uid
fct_check_losetup
losetup -fP ${IMAGE}
mkdir -p /tmp/wkmntloops
DEVLOOP=$(losetup -l | grep ${IMAGE} | awk '{print $1}')
echo $DEVLOOP
mount -o loop $DEVLOOP /tmp/wkmntloops

rm -rf /tmp/rootfs_podman
cp -vrf /tmp/wkmntloops /tmp/rootfs_podman
umount /tmp/wkmntloops
losetup -d $DEVLOOP
rmdir /tmp/wkmntloops

cat > /tmp/rootfs_podman/Dockerfile << EOF
FROM scratch
ADD . /
EOF

podman rmi bookworm 1>/dev/null 2>&1
podman build -t bookworm /tmp/rootfs_podman/

#-----------------------------------------------------------------------#

