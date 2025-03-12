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
cat > ${EXTRACT}/../${IDENT}cmd << EOF
#!/bin/bash

if [ \${#} -ne 1 ]; then
  echo cmd param needed:
  echo start stop shell canvas web_on web_off demo_ping demo_frr
  exit 
fi
cmd=\$1

IDENT="${IDENT}"
PROXYSHARE_OUT="${PROXYSHARE_OUT}"
EXTRACT="${EXTRACT}"
PROXY="\${EXTRACT}/bin/cloonix-proxymous-\${IDENT}"
CRUN="\${EXTRACT}/bin/cloonix-crun-\${IDENT}"
TMUX="\${EXTRACT}/bin/cloonix-tmux-\${IDENT}"

#-----------------------------------------------------------------------------
case \$cmd in

  start)
    exists=\$(ps axo args | grep  \${PROXY} | grep -v grep)
    if [ ! -z "\$exists" ]; then
      echo Error, process already exists:
      echo "\$exists"
      exit 1
    fi
    #------------------------------------------------------------------------
    \${PROXY} \${PROXYSHARE_OUT} \${IDENT}
    nohup \${CRUN} \\
          --cgroup-manager=disabled \\
          --log=\${EXTRACT}/log/crun.log \\
          --root=\${EXTRACT}/rootfs/tmp \\
          run -f \${EXTRACT}/config//config.json \${IDENT} \\
          > \${EXTRACT}/log/startup.log 2>&1 &
    #------------------------------------------------------------------------
    count_loop=0
    while [ ! -e "\${PROXYSHARE_OUT}/proxymous_start_status" ]; do
      sleep 1
    done
    while [ -z "\$(grep cloonix_main_server_ready \${PROXYSHARE_OUT}/proxymous_start_status)" ]; do
      count_loop=\$((count_loop+1))
      if [ \$count_loop -gt 15 ]; then
        echo Fail waiting for server
        exit 1
      fi
      cat \${PROXYSHARE_OUT}/proxymous_start_status 
      echo \$count_loop
      sleep 3
    done
    echo server started and ready
    #------------------------------------------------------------------------
  ;;

  stop)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "cloonix_cli \${IDENT} kil; sleep 1"
  ;;

  web_on)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "cloonix_cli \${IDENT} cnf web on; sleep 1"
  ;;

  web_off)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "cloonix_cli \${IDENT} cnf web off; sleep 1"
  ;;

  shell)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new
  ;;

  canvas)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "cloonix_gui \${IDENT}; sleep 1"
  ;;

  demo_ping)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "/root/ping_demo.sh; sleep 1"
  ;;

  demo_frr)
    \${TMUX} -S \${PROXYSHARE_OUT}/tmux_back new "/root/spider_frr.sh; sleep 1"
  ;;

  *)
  echo wrong cmd
  ;;
esac
EOF
#-----------------------------------------------------------------------------
chown -R ${USER}:${USER} ${EXTRACT}
chmod -R u-s ${EXTRACT}
chmod -R g-s ${EXTRACT}
chmod -R o-s ${EXTRACT}
#-----------------------------------------------------------------------------
chmod +x ${EXTRACT}/../${IDENT}cmd
printf "\n\tCHOICE:\n"
printf "./${IDENT}cmd start\n"
printf "./${IDENT}cmd stop\n"
printf "./${IDENT}cmd shell\n"
printf "./${IDENT}cmd canvas\n"
printf "./${IDENT}cmd demo_ping\n"
printf "./${IDENT}cmd demo_frr\n"
printf "./${IDENT}cmd web_on\n"
printf "./${IDENT}cmd web_off\n"
#-----------------------------------------------------------------------------
