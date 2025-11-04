#!/bin/bash
#-----------------------------------------------------------------------------
HERE=`pwd`
set -e
#-----------------------------------------------------------------------------
CLOONIX_CFG="/usr/libexec/cloonix/cloonfs/etc/cloonix.cfg"
if [ ! -e ${CLOONIX_CFG} ]; then
  echo NOT FOUND:
  echo ${CLOONIX_CFG}
  exit 1
fi
VERSION_NUM=$(cat ${CLOONIX_CFG} | grep CLOONIX_VERSION)
VERSION_NUM=${VERSION_NUM#*=}
VERSION_NUM=${VERSION_NUM%-*}
#----------------------------------------------------------------------
NAME="cloonix-toy-${VERSION_NUM}"
EXTRACT="/tmp/${NAME}"
PATCHELF="/usr/libexec/cloonix/cloonfs/bin/cloonix-patchelf"
CRUN="/usr/libexec/cloonix/cloonfs/bin/cloonix-crun"
PROXY="/usr/libexec/cloonix/cloonfs/bin/cloonix-proxymous"
XAUTH="/usr/libexec/cloonix/cloonfs/bin/xauth"
LD="/usr/libexec/cloonix/cloonfs/lib64/ld-linux-x86-64.so.2"
COMMON_LIBS="/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu"
#-----------------------------------------------------------------------------
INIT_CRUN="${HERE}/tools_crun/cloonix-init-starter-crun"
TEMPLATE="${HERE}/tools_crun/config.json.template"
SETUP="${HERE}/tools_crun/setup.sh"
SBININIT="${HERE}/tools_crun/cloonixsbininitshared"
DEMOS="${HERE}/demos"
#-----------------------------------------------------------------------------
CLOONIX="${EXTRACT}/rootfs/cloonix"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
#-----------------------------------------------------------------------------
LISTSO="libsystemd.so.0 \
        libseccomp.so.2 \
        libcap.so.2 \
        libxcb.so.1 \
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
#-----------------------------------------------------------------------------
for i in ${ZIPBASIC} ${CRUN} ${LD} ${PROXY} ${XAUTH} ${SBININIT} \
         ${TEMPLATE} ${INIT_CRUN} ${SETUP} ; do
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
if [ -e ${EXTRACT} ]; then 
  echo ${EXTRACT} already exists, removing it 
  rm -rf ${EXTRACT}
fi
#-----------------------------------------------------------------------------
cd ${HERE}
mkdir -p ${CLOONIX}
#-----------------------------------------------------------------------------
for i in "cloonfs" "insider_agents" "startup_bin"; do
  unshare -rm cp -rf /usr/libexec/cloonix/${i} ${CLOONIX}
done
#-----------------------------------------------------------------------------
SRC="/usr/libexec/cloonix/cloonfs/share"
DST="/cloonix/cloonfs/share"
sed -i s%${SRC}%${DST}% ${CLOONIX}/cloonfs/etc/fonts/fonts.conf
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
cp -f  ${SETUP}     ${EXTRACT}
cp -f  ${TEMPLATE}  ${CONFIG}
cp -f  ${INIT_CRUN} ${CONFIG}
cp -f  ${SBININIT}  ${CONFIG}
cp -rf ${DEMOS}     ${CONFIG}
#-----------------------------------------------------------------------------
${PATCHELF} --force-rpath --set-rpath ./bin                      ${BIN}/cloonix-patchelf
${PATCHELF} --set-interpreter         ./bin/ld-linux-x86-64.so.2 ${BIN}/cloonix-patchelf
${PATCHELF} --add-needed              ./bin/libm.so.6            ${BIN}/cloonix-patchelf
#---------------------------------------------------------------------------
LDLINUX="/cloonix/cloonfs/lib64/ld-linux-x86-64.so.2"
RPATH_LIB="/cloonix/cloonfs/lib:/cloonix/cloonfs/lib/x86_64-linux-gnu"
for i in $(ls ${CLOONIX}/startup_bin) ; do
  ${PATCHELF} --force-rpath --set-rpath ${RPATH_LIB} ${CLOONIX}/startup_bin/${i} 2>/dev/null
  ${PATCHELF} --set-interpreter ${LDLINUX} ${CLOONIX}/startup_bin/${i} 2>/dev/null
done
#---------------------------------------------------------------------------
set +e
LDLINUX="/lib64/ld-linux-x86-64.so.2"
RPATH_LIB="/lib:/lib/x86_64-linux-gnu"
for j in "bin" \
         "sbin" \
         "lib" \
         "lib/frr" \
         "lib/frr/modules" \
         "lib/systemd" \
         "lib/x86_64-linux-gnu" \
         "lib/x86_64-linux-gnu/rsyslog" \
         "lib/x86_64-linux-gnu/systemd" \
         "lib/x86_64-linux-gnu/qt6/plugins/platforms" \
         "lib/x86_64-linux-gnu/gconv" \
         "lib/x86_64-linux-gnu/gstreamer-1.0" \
         "lib/x86_64-linux-gnu/gstreamer1.0" \
         "lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0" \
         "lib/x86_64-linux-gnu/pipewire-0.3" ; do

  if [ -e ${CLOONIX}/cloonfs/${j} ]; then
    for i in `find ${CLOONIX}/cloonfs/${j} -maxdepth 1 -type f` ; do
      ${PATCHELF} --force-rpath --set-rpath ${RPATH_LIB} ${i} 2>/dev/null
      ${PATCHELF} --set-interpreter ${LDLINUX} ${i} 2>/dev/null
    done
  fi
done
#-----------------------------------------------------------------------------
echo ${EXTRACT}
cd /tmp
tar zcvf ${NAME}.tar.gz ${NAME}
mv ${NAME}.tar.gz ${HOME}
rm -rf ${EXTRACT}
echo END SCRIPT ${HOME}/${NAME}.tar.gz DONE 
#-----------------------------------------------------------------------------


