#!/bin/sh
#-----------------------------------------------------------------------------
EXTRACT=`pwd`
if [ $# = 1 ]; then
  IDENT=$1
else
  IDENT="nemo"
fi
cd ${EXTRACT}/..
HERE=`pwd`
mv ${EXTRACT} ${HERE}/dir_${IDENT}_extracted 
cd ${HERE}/dir_${IDENT}_extracted
EXTRACT=`pwd`
ROOTFS="${EXTRACT}/rootfs"
#-----------------------------------------------------------------------------
CLOONIX_CFG="${ROOTFS}/cloonix/cloonfs/etc/cloonix.cfg"
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
CLOONFS="${ROOTFS}/cloonix/cloonfs"
PATCHELF="${EXTRACT}/bin/cloonix-patchelf"
XAUTH="${EXTRACT}/bin/xauth"
HOST_BULK="/var/lib/cloonix/bulk"
USER_UID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$3}")
USER_GID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$4}")
PROXYSHARE_IN="/tmp/cloonix_proxymous"
PROXYSHARE_OUT="/tmp/${IDENT}_proxymous_${USER}"
PROXY="${BIN}/cloonix-proxymous-${IDENT}"
CRUN="${BIN}/cloonix-crun-${IDENT}"
#-----------------------------------------------------------------------------
mv ${CONFIG}/cloonixsbininitshared       ${ROOTFS}/cloonix/cloonfs/sbin
mv ${CONFIG}/cloonix-init-starter-crun   ${ROOTFS}/cloonix/cloonix-init-${IDENT}
mv ${CONFIG}/config.json.template        ${CONFIG}/config.json
mv ${BIN}/cloonix-crun                   ${BIN}/cloonix-crun-${IDENT}
mv ${BIN}/cloonix-proxymous              ${BIN}/cloonix-proxymous-${IDENT}
#-----------------------------------------------------------------------------
if [ -r ${HOST_BULK} ]; then
  sed -i s"%__HOST_BULK_PRESENT__%%"     ${CONFIG}/config.json
else
  sed -i s"%__HOST_BULK_PRESENT__.*%%"   ${CONFIG}/config.json
fi
#-----------------------------------------------------------------------------
if [ ! -z ${XDG_RUNTIME_DIR} ] && [ -d ${XDG_RUNTIME_DIR} ] && [ -e ${XDG_RUNTIME_DIR}/pulse ]; then
  sed -i s"%__HOST_PULSE_PRESENT__%%"                 ${CONFIG}/config.json
  sed -i s"%__XDG_RUNTIME_DIR__%${XDG_RUNTIME_DIR}%"  ${CONFIG}/config.json
else
  sed -i s"%__HOST_PULSE_PRESENT__.*%%"               ${CONFIG}/config.json
fi
#-----------------------------------------------------------------------------
sed -i s"%__IDENT__%${IDENT}%"                     ${CLOONFS}/etc/systemd/system/cloonix_server.service
sed -i s"%__IDENT__%${IDENT}%"                     ${ROOTFS}/cloonix/cloonix-init-${IDENT}
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%"     ${ROOTFS}/cloonix/cloonix-init-${IDENT}
sed -i s"%__IDENT__%${IDENT}%"                     ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%"     ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_OUT__%${PROXYSHARE_OUT}%"   ${CONFIG}/config.json
sed -i s"%__ROOTFS__%${ROOTFS}%"                   ${CONFIG}/config.json
sed -i s"%__USER_UID__%${USER_UID}%"               ${CONFIG}/config.json
sed -i s"%__USER_GID__%${USER_GID}%"               ${CONFIG}/config.json
sed -i s"%__IDENT__%${IDENT}%"                     ${CONFIG}/readme.sh
for i in $(ls ${CONFIG}/demos); do
  sed -i s"%__IDENT__%${IDENT}%" ${CONFIG}/demos/${i}/demo.sh
done
mkdir -p ${ROOTFS}/root
mv ${CONFIG}/demos ${ROOTFS}/root
#-----------------------------------------------------------------------------
for i in "cloonix-crun-${IDENT}" \
         "cloonix-proxymous-${IDENT}" \
         "xauth"; do
  ${PATCHELF} --force-rpath --set-rpath ${BIN} ${BIN}/${i}
  ${PATCHELF} --set-interpreter ${BIN}/ld-linux-x86-64.so.2 ${BIN}/${i}
done
${PATCHELF} --add-needed ${BIN}/libzstd.so.1   ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libz.so.1      ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libcrypto.so.3 ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libxcb.so.1    ${BIN}/xauth
${PATCHELF} --add-needed ${BIN}/libXdmcp.so.6  ${BIN}/xauth
#---------------------------------------------------------------------------
if [ -e /etc/resolv.conf ]; then
  cp -f /etc/resolv.conf ${CLOONFS}/etc 
fi
#---------------------------------------------------------------------------
pkill -f cloonix-proxymous-${IDENT} 1>/dev/null 2>&1
rm -rf ${PROXYSHARE_OUT} 1>/dev/null 2>&1
rm -f /tmp/xauth-${USER}-extracted
#---------------------------------------------------------------------------
mkdir -p ${EXTRACT}/worktmp
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
  fi
fi  
#-----------------------------------------------------------------------------
cat > ${EXTRACT}/../${IDENT}cmd << EOF
#!/bin/sh

if [ \${#} -ne 1 ]; then
  printf "\n\tCHOICE:\n"
  printf "./${IDENT}cmd start\n"
  printf "./${IDENT}cmd stop\n"
  printf "./${IDENT}cmd shell\n"
  printf "./${IDENT}cmd canvas\n"
  printf "./${IDENT}cmd web_on\n"
  printf "./${IDENT}cmd web_off\n"
  printf "./${IDENT}cmd demoline\n"
  printf "./${IDENT}cmd demosquare\n"
  printf "./${IDENT}cmd demospider\n"
  exit 
fi
cmd=\$1
export LC_ALL=C.UTF-8
IDENT="${IDENT}"
XAUTH_CODE=${XAUTH_CODE}
PROXYSHARE_OUT="${PROXYSHARE_OUT}"
EXTRACT="${EXTRACT}"

PROXY="\${EXTRACT}/bin/cloonix-proxymous-\${IDENT}"
CRUN="\${EXTRACT}/bin/cloonix-crun-\${IDENT}"
CRUNROOT="\${EXTRACT}/worktmp"
CGROUPM_ON="--cgroup-manager=systemd"
CGROUPM="--cgroup-manager=disabled"

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
    #------------------------------------------------------------------------
    \${CRUN} \${CGROUPM_ON} \\
             --root=\${EXTRACT}/worktmp \\
             --debug \\
             --log=\${EXTRACT}/worktmp/crun.log \\
             create -f \${EXTRACT}/config/config.json \${IDENT}
    #------------------------------------------------------------------------
    \${CRUN} \${CGROUPM} \\
             --root=\${EXTRACT}/worktmp \\
             --debug \\
             --log=\${EXTRACT}/worktmp/crun.log \\
             start \${IDENT}
    #------------------------------------------------------------------------
    count_loop=0
    while [ ! -e "\${PROXYSHARE_OUT}/proxymous_start_status" ]; do
      sleep 1
      count_loop=\$((count_loop+1))
      if [ \$count_loop -gt 0 ]; then
        echo Waiting for \${PROXY} launching, count \$count_loop.
      fi
      if [ \$count_loop -gt 3 ]; then
        echo Fail launching:
        echo \${PROXY} \${PROXYSHARE_OUT} \${IDENT}
        exit 1
      fi
    done
    echo \${PROXY} launched and OK
    echo \${XAUTH_CODE} > \${PROXYSHARE_OUT}/xauth-code-Xauthority
    cp \${XAUTHORITY} \${PROXYSHARE_OUT}/Xauthority
    while [ -z "\$(grep server_is_ready \${PROXYSHARE_OUT}/proxymous_start_status)" ]; do
      count_loop=\$((count_loop+1))
      if [ \$count_loop -gt 20 ]; then
        echo Fail waiting for server look at:
        echo \${EXTRACT}/worktmp/crun.log
        echo \${EXTRACT}/rootfs/root/cloonix-init-starter-crun.log
        echo \${EXTRACT}/rootfs/cloonix/cloonfs/var/log/syslog
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
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} kill \${IDENT} 9
    sleep 1
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} delete \${IDENT}
    rm -rf \${PROXYSHARE_OUT}
  ;;

  web_on)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} cloonix_cli \${IDENT} cnf web on
  ;;

  web_off)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} cloonix_cli \${IDENT} cnf web off
  ;;

  shell)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec -ti \${IDENT} /bin/bash 
  ;;

  canvas)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} cloonix_gui \${IDENT}
  ;;

  demoline)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} /root/demos/line/demo.sh
  ;;
  demosquare)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} /root/demos/square/demo.sh
  ;;
  demospider)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec \${IDENT} /root/demos/spider/demo.sh
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
printf "./${IDENT}cmd web_on\n"
printf "./${IDENT}cmd web_off\n"
printf "./${IDENT}cmd demoline\n"
printf "./${IDENT}cmd demosquare\n"
printf "./${IDENT}cmd demospider\n"
#-----------------------------------------------------------------------------
