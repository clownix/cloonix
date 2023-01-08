#!/bin/bash
#----------------------------------------------------------------------#
CLOONIX_BULK=/home/perrier/cloonix_data/bulk
#----------------------------------------------------------------------#
IMAGE=${CLOONIX_BULK}/bookworm.img
#----------------------------------------------------------------------#
if [ ! -e $IMAGE ]; then
  echo DOES NOT EXIST: $IMAGE
  exit 1
fi
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

rm -rf /tmp/rootfs_docker
cp -vrf /tmp/wkmntloops /tmp/rootfs_docker
umount /tmp/wkmntloops
losetup -d $DEVLOOP
rmdir /tmp/wkmntloops

cat > /tmp/rootfs_docker/Dockerfile << EOF
FROM scratch
ADD . /
EOF

docker rmi bookworm 1>/dev/null 2>&1
docker build -t bookworm /tmp/rootfs_docker/

#-----------------------------------------------------------------------#

