#!/bin/bash
#-----------------------------------------------------------------------------
HERE=`pwd`
#-----------------------------------------------------------------------------
RESULT="${HOME}/cloonix-extractor-46-00.sh"
PATCHELF="/usr/libexec/cloonix/common/cloonix-patchelf"
CRUN="/usr/libexec/cloonix/server/cloonix-crun"
PROXY="/usr/libexec/cloonix/server/cloonix-proxymous"
XAUTH="/usr/libexec/cloonix/server/xauth"
LD="/usr/libexec/cloonix/common/lib64/ld-linux-x86-64.so.2"
COMMON_LIBS="/usr/libexec/cloonix/common/lib/x86_64-linux-gnu"
#-----------------------------------------------------------------------------
INIT_CRUN="${HERE}/tools_crun/cloonix-init-starter-crun"
TEMPLATE="${HERE}/tools_crun/config.json.template"
STARTUP="${HERE}/tools_crun/readme.sh"
MAKESELF="${HERE}/makeself/makeself.sh"
#-----------------------------------------------------------------------------
EXTRACT="/tmp/dir_self_extracted"
CLOONIX="${EXTRACT}/rootfs/usr/libexec/cloonix"
VAR_CLOONIX="${EXTRACT}/rootfs/var/lib/cloonix"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
ROOT="${EXTRACT}/rootfs/root"
#-----------------------------------------------------------------------------
LISTSO="libxcb.so.1 \
        libcrypto.so.3 \
        libz.so.1 \
        libzstd.so.1 \
        libX11.so.6 \
        libXau.so.6 \
        libXext.so.6 \
        libXmuu.so.1 \
        libXdmcp.so.6 \
        libm.so.6 \
        libstdc++.so.6 \
        libgcc_s.so.1 \
        libc.so.6"
ZIPFRR="/var/lib/cloonix/bulk/zipfrr.zip"
#-----------------------------------------------------------------------------
for i in ${ZIPBASIC} ${ZIPFRR} ${CRUN} ${LD} ${PROXY} ${XAUTH} \
         ${TEMPLATE} ${INIT_CRUN} ${STARTUP} ${MAKESELF} ; do
  if [ ! -e ${i} ]; then 
    echo ${i} missing 
    exit 1
  fi
done
#-----------------------------------------------------------------------------
for i in ${LISTSO}; do
  if [ ! -e "${COMMON_LIBS}/${i}" ]; then 
    echo ${COMMON_LIBS}/${i} missing 
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
mkdir -p ${CLOONIX}
mkdir -p ${VAR_CLOONIX}/bulk
mkdir -p ${VAR_CLOONIX}/cache
for i in "run" "root" "bin" "usr" \
         "var/run" "var/log" "/var/spool/rsyslog" ; do
  mkdir -p ${EXTRACT}/rootfs/${i}
done
#-----------------------------------------------------------------------------
cp -rf /usr/libexec/cloonix/server ${CLOONIX}
cp -rf /usr/libexec/cloonix/common ${CLOONIX}
cp -f /usr/bin/cloonix* ${CLOONIX}/common
cp -f ${CLOONIX}/common/bash ${EXTRACT}/rootfs/bin
cp -f ${ZIPFRR} ${VAR_CLOONIX}/bulk
cp -f ${HERE}/tools_crun/ping_demo.sh ${EXTRACT}/rootfs/root
cp -f ${HERE}/tools_crun/spider_frr.sh ${EXTRACT}/rootfs/root
#-----------------------------------------------------------------------------
mkdir -p ${CONFIG}
mkdir -p ${BIN}
#---------------------------------------------------------------------------
for i in ${LISTSO}; do
  cp -f ${COMMON_LIBS}/${i} ${BIN} 
done
#---------------------------------------------------------------------------
for i in ${LD} ${CRUN} ${PROXY} ${PATCHELF} ${XAUTH} ; do
  cp -f ${i} ${BIN}
done
#-----------------------------------------------------------------------------
cp -f ${TEMPLATE}  ${CONFIG}
cp -f ${INIT_CRUN} ${CONFIG}
cp -f ${STARTUP}   ${CONFIG}
#-----------------------------------------------------------------------------
${PATCHELF} --force-rpath --set-rpath ./bin                      ${BIN}/cloonix-patchelf
${PATCHELF} --set-interpreter         ./bin/ld-linux-x86-64.so.2 ${BIN}/cloonix-patchelf
${PATCHELF} --add-needed              ./bin/libm.so.6            ${BIN}/cloonix-patchelf
#---------------------------------------------------------------------------
mkdir -p ${EXTRACT}/rootfs/var/run/netns
#-----------------------------------------------------------------------------
OPTIONS="--nooverwrite --notemp --nomd5 --nocrc --tar-quietly --quiet"
${MAKESELF} ${OPTIONS} ${EXTRACT} ${RESULT} "cloonix" ./config/readme.sh
#-----------------------------------------------------------------------------
rm -rf ${EXTRACT}
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------


