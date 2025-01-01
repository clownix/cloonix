#!/bin/bash
#-----------------------------------------------------------------------------
HERE=`pwd`
if [ ${#} = 1 ]; then
  IDENT=$1
else
  IDENT="nemo"
fi


CLOONIX_CFG="/usr/libexec/cloonix/common/etc/cloonix.cfg"
LIST=$(cat $CLOONIX_CFG |grep CLOONIX_NET: | awk '{print $2}')
FOUND="no"
for i in $LIST ; do
  if [ "${IDENT}" = "${i}" ]; then
    echo FOUND IN CONFIG: ${IDENT}
    FOUND="yes"
    break
  fi
done
if [ "${FOUND}" = "no" ]; then
  echo NOT FOUND, CHOOSE:
  echo $LIST
  exit 1
fi

PATCHELF="/usr/libexec/cloonix/common/cloonix-patchelf"
XAUTH="/usr/libexec/cloonix/server/xauth"
TMUXORIG="cloonix-tmux"
TMUXCUST="cloonix-tmux-${IDENT}"
TMUX="/usr/libexec/cloonix/server/${TMUXORIG}"
CRUNORIG="cloonix-crun"
CRUNCUST="cloonix-crun-${IDENT}"
CRUN="/usr/libexec/cloonix/server/${CRUNORIG}"
PROXYORIG="cloonix-proxy-crun-access"
PROXYCUST="cloonix-proxy-${IDENT}"
PROXY="/usr/libexec/cloonix/server/proxy/${PROXYORIG}"
LD="/usr/libexec/cloonix/common/lib64/ld-linux-x86-64.so.2"
BASH="/usr/libexec/cloonix/common/bash"
COMMON_LIBS="/usr/libexec/cloonix/common/lib/x86_64-linux-gnu"
#-----------------------------------------------------------------------------
INIT_CRUNORIG="cloonix-init-starter-crun"
INIT_CRUNCUST="cloonix-init-${IDENT}"
INIT_CRUN_PATH="${HERE}/tools_crun/${INIT_CRUNORIG}"
TEMPLATE="config.json.template"
TEMPLATE_PATH="${HERE}/tools_crun/${TEMPLATE}"
STARTUP="readme.sh"
STARTUP_PATH="${HERE}/tools_crun/${STARTUP}"
MAKESELF="${HERE}/makeself/makeself.sh"
#-----------------------------------------------------------------------------
EXTRACT="/tmp/extract${IDENT}"
CLOONIX="${EXTRACT}/rootfs/usr/libexec/cloonix"
VAR_CLOONIX="${EXTRACT}/rootfs/var/lib/cloonix"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
ROOT="${EXTRACT}/rootfs/root"
#-----------------------------------------------------------------------------
LISTSO="libcrypto.so.3 \
        libz.so.1 \
        libzstd.so.1 \
        libtinfo.so.6 \
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
        libmd.so.0 \
        libevent_core-2.1.so.7 \
        libresolv.so.2"
ZIPFRR="/var/lib/cloonix/bulk/zipfrr.zip"
RESULT="${HOME}/extract_${IDENT}.sh"
#-----------------------------------------------------------------------------
for i in ${ZIPBASIC} ${ZIPFRR} ${CRUN} ${LD} ${XAUTH} ${TMUX} \
         ${PROXY} ${TEMPLATE_PATH} ${INIT_CRUN_PATH} \
         ${STARTUP_PATH} ${MAKESELF} ; do
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
NSERV="${CLOONIX}/server"
mv ${NSERV}/${TMUXORIG} ${NSERV}/${TMUXCUST}
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
for i in ${LD} ${CRUN} ${PROXY} ${BASH} ${XAUTH} \
         ${TMUX} ${PATCHELF} ; do
  cp -f ${i} ${BIN}
done
mv ${BIN}/${TMUXORIG} ${BIN}/${TMUXCUST}
mv ${BIN}/${CRUNORIG} ${BIN}/${CRUNCUST}
mv ${BIN}/${PROXYORIG} ${BIN}/${PROXYCUST}
#-----------------------------------------------------------------------------
cp -f ${TEMPLATE_PATH}  ${CONFIG}
cp -f ${INIT_CRUN_PATH} ${CONFIG}
cp -f ${STARTUP_PATH}   ${CONFIG}
#-----------------------------------------------------------------------------
mv ${CONFIG}/${INIT_CRUNORIG} ${CONFIG}/${INIT_CRUNCUST}
#-----------------------------------------------------------------------------
for i in "${STARTUP}" \
         "${INIT_CRUNCUST}" \
         "${TEMPLATE}" ; do
  sed -i s"/__IDENT__/${IDENT}/"g ${CONFIG}/${i}
done
#-----------------------------------------------------------------------------
for i in "ping_demo.sh" \
         "spider_frr.sh" ; do
  sed -i s"/__IDENT__/${IDENT}/"g ${ROOT}/${i}
done
#-----------------------------------------------------------------------------
${PATCHELF} --force-rpath --set-rpath ./bin                      ${BIN}/cloonix-patchelf
${PATCHELF} --set-interpreter         ./bin/ld-linux-x86-64.so.2 ${BIN}/cloonix-patchelf
${PATCHELF} --add-needed              ./bin/libm.so.6            ${BIN}/cloonix-patchelf
#---------------------------------------------------------------------------
OPTIONS="--nooverwrite --notemp --nomd5 --nocrc --tar-quietly --quiet"
${MAKESELF} ${OPTIONS} ${EXTRACT} ${RESULT} "${IDENT}" ./config/${STARTUP}
#-----------------------------------------------------------------------------
rm -rf ${EXTRACT}
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------


