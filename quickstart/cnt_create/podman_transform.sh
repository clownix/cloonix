#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
BULK="/var/lib/cloonix/bulk"
#----------------------------------------------------------------------#
if [ $# = 1 ]; then
  NAME_ZIP=$1
  PATH_ZIP=${BULK}/$1
  PREFIX=${NAME_ZIP%%.*}
  SUFFIX=${NAME_ZIP##*.}
  ROOTFS=/tmp/rootfs_${PREFIX}
  if [ "$SUFFIX" != "zip" ]; then
    printf "\nSUFFIX MUST BE .zip\n\n"
    exit 1
  elif [ ! -e $PATH_ZIP ]; then
    printf "\nDOES NOT EXIST: $PATH_ZIP\n\n"
    exit 1
  elif  [[ $(podman images | egrep -c ${PREFIX}) != 0 ]]; then
    printf "\nPODMAN ${PREFIX} EXISTS ALREADY:\n"
    printf "$(podman images | grep ${PREFIX})\n\n"
    exit 1
  else
    printf "\nCREATING PODMAN FROM ${PATH_ZIP}\n\n"
  fi
else
  printf "\nZIP NAME NEEDED, EXAMPLE:\n"
  printf "$0 bookworm.zip\n\n"
  exit 1
fi
#----------------------------------------------------------------------#
rm -rf ${ROOTFS}
mkdir -p ${ROOTFS}
cp ${PATH_ZIP} ${ROOTFS}
cd ${ROOTFS}
unzip ${NAME_ZIP}
cd ${HERE}
rm -f ${ROOTFS}/${NAME_ZIP}
cat > ${ROOTFS}/Dockerfile << EOF
FROM scratch
ADD . /
EOF
podman build -t ${PREFIX} ${ROOTFS}
echo PODMAN ${PREFIX} DONE



# echo THEN CALL PODMAN:
# printf "\nsudo podman run -it localhost/${PREFIX} /bin/bash\n\n"
# echo FOR THE CASE OF THE cnt_cloonix, CONTAINER TO RUN CLOONIX IN PODMAN:
# echo FIRST PERMIT XHOST:
# printf "\nxhost +\n"
# echo THEN CALL PODMAN WITH MORE PARAMS:
# printf "\nsudo podman run -it --privileged -e DISPLAY=:0 -v /tmp/.X11-unix:/tmp/.X11-unix -v /var/lib/cloonix/bulk:/var/lib/cloonix/bulk localhost/${PREFIX} /bin/bash\n"
# echo
# sudo podman save -o ${PREFIX}.tar localhost/${PREFIX}
# sudo podman load -i ${PREFIX}.tar

#-----------------------------------------------------------------------#


exit

getfacl /dev/kvm
getfacl /dev/vhost-net
getfacl /dev/net/tun

sudo setfacl -R -m u:${USER}:rw /dev/kvm
sudo setfacl -R -m u:${USER}:rw /dev/vhost-net 
sudo setfacl -R -m u:${USER}:rw /dev/net/tun

getfacl /dev/kvm
getfacl /dev/vhost-net
getfacl /dev/net/tun

podman run -it --privileged \
-e DISPLAY=:0 -v /tmp/.X11-unix:/tmp/.X11-unix \
-v /var/lib/cloonix/bulk:/var/lib/cloonix/bulk \
-v /lib/modules:/lib/modules \
localhost/cnt_cloonix /bin/bash


When inside the container:

mkdir /tmp/sysfs
mount --rbind /sys /tmp/sysfs
cloonix_net nemo
cloonix_gui nemo



 nsenter -a -t <pid> /bin/bash

