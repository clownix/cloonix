#!/bin/bash
#-----------------------------------------------------------------------------
HERE=`pwd`
ZIPCLOONIX="/var/lib/cloonix/bulk/zipcloonix.zip"
ZIPBASIC="/var/lib/cloonix/bulk/zipbasic.zip"
ZIPFRR="/var/lib/cloonix/bulk/zipfrr.zip"
OPENWRT="/var/lib/cloonix/bulk/openwrt.qcow2"
UNSHARE="/usr/libexec/cloonix/common/unshare"
UNZIP="/usr/libexec/cloonix/common/unzip"
CRUN="/usr/libexec/cloonix/server/cloonix-crun"
PTYS="/usr/libexec/cloonix/server/cloonix-scriptpty"
LIBC="/usr/libexec/cloonix/common/lib/x86_64-linux-gnu/libc.so.6"
LD="/usr/libexec/cloonix/common/lib64/ld-linux-x86-64.so.2"
TEMPLATE="${HERE}/tools_crun/config.json.template"
DEMOS="${HERE}/../demos/"
CRUN_STARTUP="${HERE}/tools_crun/crun_container_startup.sh"
README="${HERE}/tools_crun/readme.sh"
MAKESELF="${HERE}/makeself/makeself.sh"
EXTRACT="/tmp/self_extracting_rootfs_dir"
ROOTFS="${EXTRACT}/rootfs"
BULK="${EXTRACT}/bulk"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
RESULT="${HOME}/self_extracting_cloonix.sh"
#-----------------------------------------------------------------------------
for i in ${ZIPCLOONIX} ${ZIPBASIC} ${ZIPFRR} ${OPENWRT} \
         ${UNSHARE} ${UNZIP} ${CRUN} ${PTYS} \
         ${TEMPLATE} ${CRUN_STARTUP} ${README} \
         ${MAKESELF} ${LIBC} ${LD}; do
  if [ ! -e ${i} ]; then 
    echo ${i} missing 
    exit 1
  fi
done
#-----------------------------------------------------------------------------
for i in ${EXTRACT} ${RESULT} ; do
  if [ -e ${i} ]; then 
    echo ${i} already exists, removing it 
    rm -rf ${i}
    sleep 2
  fi
done
#-----------------------------------------------------------------------------
mkdir -p ${ROOTFS}
mkdir -p ${BULK}
mkdir -p ${CONFIG}
mkdir -p ${BIN}
#-----------------------------------------------------------------------------
cd ${ROOTFS}
${UNSHARE} -rm ${UNZIP} ${ZIPCLOONIX}
cd ${HERE}
#---------------------------------------------------------------------------
cp -f ${CRUN} ${BIN}
cp -f ${PTYS} ${BIN}
cp -f ${LIBC} ${BIN}
cp -f ${LD}   ${BIN}
cp -f ${ZIPBASIC} ${BULK}
cp -f ${ZIPFRR} ${BULK}
cp -f ${OPENWRT}  ${BULK}
cp -f ${TEMPLATE} ${CONFIG}
cp -rf ${DEMOS} ${EXTRACT}
cp -f ${CRUN_STARTUP} ${EXTRACT}
cp -f ${README} ${EXTRACT}
#---------------------------------------------------------------------------
OPTIONS="--nocomp --nooverwrite --notemp --nomd5 --nocrc"
${MAKESELF} ${OPTIONS} ${EXTRACT} ${RESULT} "Cloonix" ./readme.sh 
#-----------------------------------------------------------------------------
rm -rf ${EXTRACT}
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------


