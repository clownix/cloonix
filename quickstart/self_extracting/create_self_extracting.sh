#!/bin/bash
#-----------------------------------------------------------------------------
HERE=`pwd`
PATCHELF="/usr/libexec/cloonix/common/cloonix-patchelf"
UNZIP="/usr/libexec/cloonix/common/unzip"
CRUN="/usr/libexec/cloonix/server/cloonix-crun"
PTYS="/usr/libexec/cloonix/server/cloonix-scriptpty"
LD="/usr/libexec/cloonix/common/lib64/ld-linux-x86-64.so.2"
XAUTH="/usr/libexec/cloonix/common/xauth"
BASH="/usr/libexec/cloonix/common/bash"
COMMON_LIBS="/usr/libexec/cloonix/common/lib/x86_64-linux-gnu"
#-----------------------------------------------------------------------------
TEMPLATE="${HERE}/tools_crun/config.json.template"
CRUN_STARTUP="${HERE}/tools_crun/crun_container_startup.sh"
README="${HERE}/tools_crun/readme.sh"
MAKESELF="${HERE}/makeself/makeself.sh"
#-----------------------------------------------------------------------------
EXTRACT="/tmp/self_extracting_rootfs_dir"
CLOONIX="${EXTRACT}/rootfs/usr/libexec/cloonix"
VAR_CLOONIX="${EXTRACT}/rootfs/var/lib/cloonix"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
#-----------------------------------------------------------------------------
LISTSO="libtinfo.so.6 \
        libstdc++.so.6 \
        libgcc_s.so.1 \
        libc.so.6 \
        libm.so.6 \
        libX11.so.6 \
        libXau.so.6 \
        libXext.so.6 \
        libXmuu.so.1 \
        libxcb.so.1 \
        libXdmcp.so.6 \
        libbsd.so.0 \
        libmd.so.0"
ZIPFRR="/var/lib/cloonix/bulk/zipfrr.zip"
RESULT="${HOME}/self_extracting_cloonix.sh"
#-----------------------------------------------------------------------------
for i in ${ZIPBASIC} ${ZIPFRR} ${UNZIP} ${CRUN} ${PTYS} ${LD} \
         ${TEMPLATE} ${CRUN_STARTUP} ${README} ${MAKESELF} ; do
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
cp -rf /usr/libexec/cloonix/server ${CLOONIX}
cp -rf /usr/libexec/cloonix/common ${CLOONIX}
cp -f /usr/bin/cloonix* ${CLOONIX}/common
cp -f ${CLOONIX}/common/bash ${EXTRACT}/rootfs/bin
cp -f ${ZIPFRR} ${VAR_CLOONIX}/bulk
cp -f ${HERE}/tools_crun/ping_demo.sh ${EXTRACT}/rootfs/root
cp -f ${HERE}/tools_crun/spider_frr.sh ${EXTRACT}/rootfs/root
#-----------------------------------------------------------------------------
chown -R ${USER}:${USER} ${EXTRACT}/rootfs
#-----------------------------------------------------------------------------
mkdir -p ${CONFIG}
mkdir -p ${BIN}
#---------------------------------------------------------------------------
for i in ${LISTSO}; do
  cp -f ${COMMON_LIBS}/${i} ${BIN} 
done
for i in ${LD} ${CRUN} ${PTYS} ${XAUTH} ${BASH} ; do
  cp -f ${i} ${BIN}
done
#-----------------------------------------------------------------------------
cp -f ${TEMPLATE} ${CONFIG}
cp -f ${CRUN_STARTUP} ${EXTRACT}
cp -f ${README} ${EXTRACT}
#-----------------------------------------------------------------------------
for i in "cloonix-crun" "cloonix-scriptpty" "xauth" "bash" ; do
  ${PATCHELF} --force-rpath --set-rpath ./bin ${BIN}/${i}
  ${PATCHELF} --set-interpreter ./bin/ld-linux-x86-64.so.2 ${BIN}/${i}
done
#---------------------------------------------------------------------------
OPTIONS="--nooverwrite --notemp --nomd5 --nocrc --tar-quietly --quiet"
${MAKESELF} ${OPTIONS} ${EXTRACT} ${RESULT} "Cloonix" ./readme.sh 
#-----------------------------------------------------------------------------
rm -rf ${EXTRACT}
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------


