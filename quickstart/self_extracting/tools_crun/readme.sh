#!/bin/bash
EXTRACT=`pwd`
BIN="${EXTRACT}/bin"
PATCHELF="${EXTRACT}/bin/cloonix-patchelf"
ROOTFS="${EXTRACT}/rootfs"
INIT_CRUN="${EXTRACT}/config/cloonix-init-__IDENT__"
CRUN="${EXTRACT}/bin/cloonix-crun-__IDENT__"
TMUX="${EXTRACT}/bin/cloonix-tmux-__IDENT__"
PROXY="${EXTRACT}/bin/cloonix-proxy-__IDENT__"
CONFIG="${EXTRACT}/config/config.json"
PROXYSHARE="/tmp/__IDENT___proxy_share"
HOST_BULK="/var/lib/cloonix/bulk"
#---------------------------------------------------------------------------
cp -f ${EXTRACT}/config/tmux.conf ${ROOTFS}/root/.tmux.conf
#---------------------------------------------------------------------------
for i in "cloonix-crun-__IDENT__" \
         "cloonix-proxy-__IDENT__" \
         "cloonix-tmux-__IDENT__" \
         "bash" \
         "xauth" ; do
  ${PATCHELF} --force-rpath --set-rpath ${BIN} ${BIN}/${i}
  ${PATCHELF} --set-interpreter ${BIN}/ld-linux-x86-64.so.2 ${BIN}/${i}
done
#---------------------------------------------------------------------------
${PATCHELF} --add-needed ${BIN}/libzstd.so.1   ${BIN}/cloonix-proxy-__IDENT__
${PATCHELF} --add-needed ${BIN}/libz.so.1      ${BIN}/cloonix-proxy-__IDENT__
${PATCHELF} --add-needed ${BIN}/libcrypto.so.3 ${BIN}/cloonix-proxy-__IDENT__
#---------------------------------------------------------------------------
${PATCHELF} --add-needed ${BIN}/libxcb.so.1    ${BIN}/xauth
${PATCHELF} --add-needed ${BIN}/libXdmcp.so.6  ${BIN}/xauth
#---------------------------------------------------------------------------
pkill -f cloonix-proxy-__IDENT__ 1>/dev/null 2>&1
${TMUX} -S ${PROXYSHARE}/tmux_back kill-server 1>/dev/null 2>&1
rm -rf ${PROXYSHARE} 1>/dev/null 2>&1
mkdir -p ${PROXYSHARE}/X11-unix
mkdir -p ${EXTRACT}/log
#-----------------------------------------------------------------------------
cp -f ${EXTRACT}/config/config.json.template ${CONFIG}
cp -f ${XAUTHORITY} ${PROXYSHARE}/Xauthority
#-----------------------------------------------------------------------------
sed -i s"%__HOSTNAME__%${HOSTNAME}%"   ${INIT_CRUN}
sed -i s"%__DISPLAY__%${DISPLAY}%"     ${INIT_CRUN}
cp -f ${INIT_CRUN} ${ROOTFS}/usr/libexec/cloonix/server
#-----------------------------------------------------------------------------
if [ -r ${HOST_BULK} ]; then
  sed -i s"%__HOST_BULK_PRESENT__%%" ${CONFIG}
else
  sed -i s"%__HOST_BULK_PRESENT__.*%%" ${CONFIG}
fi
#-----------------------------------------------------------------------------
sed -i s"%__PROXYSHARE__%${PROXYSHARE}%" ${CONFIG}
sed -i s"%__ROOTFS__%${ROOTFS}%"         ${CONFIG}
sed -i s"%__UID__%${UID}%"               ${CONFIG}
#-----------------------------------------------------------------------------
if [ ! -e /usr/bin/newgidmap ] || [ ! -e /usr/bin/newuidmap ]; then
  printf "\n\nMISSING uidmap package!!!!!!!!!!!!!!\n"
  echo NEEDS newgidmap and newuidmap to work
  printf "for debian: \napt-get install uidmap\n\n"
  exit 1
else
  ${PROXY} ${PROXYSHARE}
    nohup ${CRUN} \
           --log=${EXTRACT}/log/crun.log \
           --root=${ROOTFS}/tmp \
           run -f ${CONFIG} __IDENT__ > ${EXTRACT}/log/startup.log 2>&1 &

#    nohup ${CRUN} \
#           --cgroup-manager=disabled \
#           --rootless=100000 \
#           --log=${EXTRACT}/log/crun.log \
#           --root=${ROOTFS}/tmp \
#           run -f ${CONFIG} __IDENT__ > ${EXTRACT}/log/startup.log 2>&1 &
fi
#-----------------------------------------------------------------------------
while [ ! -e "${PROXYSHARE}/tmux_back" ]; do 
  sleep 1
done
sleep 1
#-----------------------------------------------------------------------------
cat > ${EXTRACT}/../__IDENT___cmd << EOF
#!/bin/bash

if [ \${#} -ne 1 ]; then
  echo cmd param needed:
  echo sh gui ping frr kil
  exit 
fi
cmd=\$1
case \$cmd in

  sh)
    ${TMUX} -S ${PROXYSHARE}/tmux_back new
  ;;

  gui)
    ${TMUX} -S ${PROXYSHARE}/tmux_back new 'cloonix_gui __IDENT__'
  ;;

  ping)
    ${TMUX} -S ${PROXYSHARE}/tmux_back new '/root/ping_demo.sh'
  ;;

  frr)
    ${TMUX} -S ${PROXYSHARE}/tmux_back new '/root/spider_frr.sh'
  ;;

  kil)
    ${TMUX} -S ${PROXYSHARE}/tmux_back new 'cloonix_cli __IDENT__ kil'
  ;;

  *)
  echo wrong cmd param:
  echo sh gui ping frr kil
  exit 
  ;;
esac
EOF

chmod +x ${EXTRACT}/../__IDENT___cmd

printf "\n\tFor the gui canvas:\n"
printf "./__IDENT___cmd gui\n"
printf "\n\tFor the ping demo:\n"
printf "./__IDENT___cmd ping\n"
printf "\n\tFor the frr demo:\n"
printf "./__IDENT___cmd frr\n"
printf "\n\tFor the killing of all:\n"
printf "./__IDENT___cmd kil\n"
printf "\n\tFor a shell in the crun container:\n"
printf "./__IDENT___cmd sh\n"
printf "\n\t\tEnter mode to scroll the history buffer:\n"
printf "\t\tCtrl-space Pg-up\n"
printf "\n\t\tReturn to normal mode:\n"
printf "\t\tq\n\n"
