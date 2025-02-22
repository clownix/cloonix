#!/bin/bash
#-----------------------------------------------------------------------------
EXTRACT=`pwd`
ROOTFS="${EXTRACT}/rootfs"
if [ $# = 1 ]; then
  IDENT=$1
else
  IDENT="nemo"
fi
#-----------------------------------------------------------------------------
printf "\n\tChosen ident: $IDENT.\n"
#-----------------------------------------------------------------------------
CLOONIX_CFG="${ROOTFS}/usr/libexec/cloonix/common/etc/cloonix.cfg"
LIST=$(cat ${CLOONIX_CFG} |grep CLOONIX_NET: | awk '{print $2}')
FOUND="no"
for i in ${LIST} ; do
  if [ "${IDENT}" = "${i}" ]; then
    FOUND="yes"
    break
  fi
done
if [ "${FOUND}" = "no" ]; then
  printf "\n\t ${IDENT} not found,\nlist: ${LIST}\n\n"
  exit 1
fi
#-----------------------------------------------------------------------------
BIN="${EXTRACT}/bin"
CONFIG="${EXTRACT}/config"
SERVER="${ROOTFS}/usr/libexec/cloonix/server"
PATCHELF="${EXTRACT}/bin/cloonix-patchelf"
XAUTH="${EXTRACT}/bin/xauth"
HOST_BULK="/var/lib/cloonix/bulk"
KVM_GID=$(cat /etc/group |grep ^kvm | awk -F : "{print \$3}")
USER_UID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$3}")
USER_GID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$4}")
PROXYSHARE_IN="/tmp/cloonix_proxymous"
PROXYSHARE_OUT="/tmp/${IDENT}_proxymous_${USER}"
PROXY="${BIN}/cloonix-proxymous-${IDENT}"
TMUX="${BIN}/cloonix-tmux-${IDENT}"
CRUN="${BIN}/cloonix-crun-${IDENT}"
#-----------------------------------------------------------------------------
mv ${CONFIG}/cloonix-init-starter-crun   ${SERVER}/cloonix-init-${IDENT}
mv ${CONFIG}/config.json.template        ${CONFIG}/config.json
mv ${CONFIG}/tmux.conf                   ${ROOTFS}/root/.tmux.conf
mv ${BIN}/cloonix-tmux                   ${BIN}/cloonix-tmux-${IDENT}
mv ${BIN}/cloonix-crun                   ${BIN}/cloonix-crun-${IDENT}
mv ${BIN}/cloonix-proxymous              ${BIN}/cloonix-proxymous-${IDENT}
mv ${SERVER}/cloonix-tmux                ${SERVER}/cloonix-tmux-${IDENT} 
#-----------------------------------------------------------------------------
if [ -r ${HOST_BULK} ]; then
  sed -i s"%__HOST_BULK_PRESENT__%%"     ${CONFIG}/config.json
else
  sed -i s"%__HOST_BULK_PRESENT__.*%%"   ${CONFIG}/config.json
fi
sed -i s"%__IDENT__%${IDENT}%"                     ${SERVER}/cloonix-init-${IDENT}
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%"     ${SERVER}/cloonix-init-${IDENT}
sed -i s"%__IDENT__%${IDENT}%"                     ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%"     ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_OUT__%${PROXYSHARE_OUT}%"   ${CONFIG}/config.json
sed -i s"%__ROOTFS__%${ROOTFS}%"                   ${CONFIG}/config.json
sed -i s"%__UID__%${UID}%"                         ${CONFIG}/config.json
sed -i s"%__KVM_GID__%${KVM_GID}%"                 ${CONFIG}/config.json
sed -i s"%__USER_UID__%${USER_UID}%"               ${CONFIG}/config.json
sed -i s"%__USER_GID__%${USER_GID}%"               ${CONFIG}/config.json
sed -i s"%__XDG_RUNTIME_DIR__%${XDG_RUNTIME_DIR}%" ${CONFIG}/config.json
sed -i s"%__IDENT__%${IDENT}%"                     ${CONFIG}/readme.sh
sed -i s"%__IDENT__%${IDENT}%"                     ${ROOTFS}/root/ping_demo.sh
sed -i s"%__IDENT__%${IDENT}%"                     ${ROOTFS}/root/spider_frr.sh
#-----------------------------------------------------------------------------
for i in "cloonix-crun-${IDENT}" \
         "cloonix-proxymous-${IDENT}" \
         "cloonix-tmux-${IDENT}" \
         "bash" \
         "xauth" ; do
  ${PATCHELF} --force-rpath --set-rpath ${BIN} ${BIN}/${i}
  ${PATCHELF} --set-interpreter ${BIN}/ld-linux-x86-64.so.2 ${BIN}/${i}
done
${PATCHELF} --add-needed ${BIN}/libzstd.so.1   ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libz.so.1      ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libcrypto.so.3 ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libxcb.so.1    ${BIN}/xauth
${PATCHELF} --add-needed ${BIN}/libXdmcp.so.6  ${BIN}/xauth
#---------------------------------------------------------------------------
pkill -f cloonix-proxymous-${IDENT} 1>/dev/null 2>&1
${TMUX} -S ${PROXYSHARE_OUT}/tmux_back kill-server 1>/dev/null 2>&1
rm -rf ${PROXYSHARE_OUT} 1>/dev/null 2>&1
rm -f /tmp/xauth-${USER}-extracted
#---------------------------------------------------------------------------
mkdir -p ${PROXYSHARE_OUT}/X11-unix
mkdir -p ${EXTRACT}/log
#-----------------------------------------------------------------------------
if [ -z ${DISPLAY} ]; then
  echo BEWARE: DISPLAY NOT INITIALIZED, GUI WILL FAIL
else
  ${XAUTH} nextract /tmp/xauth-${USER}-extracted ${DISPLAY}
  if [ ! -e /tmp/xauth-${USER}-extracted ]; then
    echo ERROR CREATE XAUTHORITY
  else
    XAUTH_EXTRACT=$(cat /tmp/xauth-${USER}-extracted)
    XAUTH_CODE=${XAUTH_EXTRACT##* }
    echo "${XAUTH_CODE}">${PROXYSHARE_OUT}/xauth-code-Xauthority
  fi
fi
#-----------------------------------------------------------------------------
${PROXY} ${PROXYSHARE_OUT} ${IDENT}
  nohup ${CRUN} \
         --cgroup-manager=disabled \
         --log=${EXTRACT}/log/crun.log \
         --root=${ROOTFS}/tmp \
         run -f ${CONFIG}/config.json ${IDENT} \
          > ${EXTRACT}/log/startup.log 2>&1 &
#-----------------------------------------------------------------------------
count_loop=0
while [ ! -e "${PROXYSHARE_OUT}/tmux_back" ]; do 
  count_loop=$((count_loop+1))
  if [ $count_loop -gt 10 ]; then
    echo Fail ${PROXYSHARE_OUT}/tmux_back waiting
    break
  fi
  sleep 1
done
sleep 1
#-----------------------------------------------------------------------------
cat > ${EXTRACT}/../${IDENT}cmd << EOF
#!/bin/bash

if [ \${#} -ne 1 ]; then
  echo cmd param needed:
  echo sh gui ping frr kil
  exit 
fi
cmd=\$1
case \$cmd in

  sh)
    ${TMUX} -S ${PROXYSHARE_OUT}/tmux_back new
  ;;
  gui)
    ${TMUX} -S ${PROXYSHARE_OUT}/tmux_back new 'cloonix_gui ${IDENT}'
  ;;
  ping)
    ${TMUX} -S ${PROXYSHARE_OUT}/tmux_back new '/root/ping_demo.sh'
  ;;
  frr)
    ${TMUX} -S ${PROXYSHARE_OUT}/tmux_back new '/root/spider_frr.sh'
  ;;
  kil)
    ${TMUX} -S ${PROXYSHARE_OUT}/tmux_back new 'cloonix_cli ${IDENT} kil'
  ;;
  *)
  echo wrong cmd param:
  echo sh gui ping frr kil
  exit 
  ;;
esac
EOF

chmod +x ${EXTRACT}/../${IDENT}cmd

printf "\n\tFor the gui canvas:\n"
printf "./${IDENT}cmd gui\n"
printf "\n\tFor the ping demo:\n"
printf "./${IDENT}cmd ping\n"
printf "\n\tFor the frr demo:\n"
printf "./${IDENT}cmd frr\n"
printf "\n\tFor the killing of all:\n"
printf "./${IDENT}cmd kil\n"
printf "\n\tFor a shell in the crun container:\n"
printf "./${IDENT}cmd sh\n"
printf "\n\tWhile in tmux shell, to scroll the history:\n"
printf "\tCtrl-space Pg-up\n"
printf "\n\tThen to return to normal mode:\n"
printf "\tq\n\n"
