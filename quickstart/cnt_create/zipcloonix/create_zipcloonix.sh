#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
ROOTFS="/tmp/zipcloonix_rootfs"
BULK="/var/lib/cloonix/bulk"
BASIC="zipbasic.zip"
CLOONIX="zipcloonix.zip"
BUNDLE_PATH="/home/perrier"
BUNDLE="cloonix-bundle-40-00-amd64"
#----------------------------------------------------------------------------#
if [ ! -e ${BUNDLE_PATH}/${BUNDLE}.tar.gz ]; then
  echo MISSING ${BUNDLE_PATH}/${BUNDLE}.tar.gz
  exit 1
fi
if [ ! -e ${BULK}/${BASIC} ]; then
  echo MISSING ${BULK}/${BASIC}
  exit 1
fi
rm -rf ${ROOTFS}
rm -f ${BULK}/${CLOONIX}
mkdir -p ${ROOTFS}
cd ${ROOTFS}
echo QUIET UNZIP OF ${BULK}/${BASIC}
unshare -rm unzip -q ${BULK}/${BASIC}
#----------------------------------------------------------------------------#
cp -f ${BUNDLE_PATH}/${BUNDLE}.tar.gz ${ROOTFS}
unshare -rm chroot ${ROOTFS} /bin/bash -c "tar --no-same-owner -xvf ${BUNDLE}.tar.gz"
unshare -rm chroot ${ROOTFS} /bin/bash -c "cd ${BUNDLE}; ./install_cloonix"
rm -f ${ROOTFS}/${BUNDLE}.tar.gz
rm -rf ${ROOTFS}/${BUNDLE}
#----------------------------------------------------------------------------#
cd ${ROOTFS}
echo QUIET ZIP OF ${BULK}/${CLOONIX}
unshare -rm  zip -yrq ${BULK}/${CLOONIX} .
echo DONE ${BULK}/${CLOONIX}
#----------------------------------------------------------------------------#


