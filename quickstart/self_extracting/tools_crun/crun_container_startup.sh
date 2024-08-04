#!/bin/bash
#-----------------------------------------------------------------------------
EXTRACT=`pwd`
ROOTFS="${EXTRACT}/rootfs"
BULK="${EXTRACT}/bulk"
CONFIG="${EXTRACT}/config/config.json"
TEMPLATE="${EXTRACT}/config/config.json.template"
CRUN="${EXTRACT}/bin/cloonix-crun"
PTYS="${EXTRACT}/bin/cloonix-scriptpty"
XAUTH="${ROOTFS}/usr/libexec/cloonix/common/xauth"
GREP="${ROOTFS}/usr/libexec/cloonix/common/grep"
PATCHELF="${ROOTFS}/usr/libexec/cloonix/common/cloonix-patchelf"
LIBCDIR="./bin"
#-----------------------------------------------------------------------------
CAPA="[\"CAP_CHOWN\",\"CAP_DAC_OVERRIDE\",\"CAP_DAC_READ_SEARCH\",\
\"CAP_FOWNER\",\"CAP_FSETID\",\"CAP_KILL\",\"CAP_SETGID\",\"CAP_SETUID\",\
\"CAP_SETPCAP\",\"CAP_LINUX_IMMUTABLE\",\"CAP_NET_BIND_SERVICE\",\
\"CAP_NET_BROADCAST\",\"CAP_NET_ADMIN\",\"CAP_NET_RAW\",\"CAP_IPC_LOCK\",\
\"CAP_IPC_OWNER\",\"CAP_SYS_MODULE\",\"CAP_SYS_RAWIO\",\"CAP_SYS_CHROOT\",\
\"CAP_SYS_PTRACE\",\"CAP_SYS_PACCT\",\"CAP_SYS_ADMIN\",\"CAP_SYS_BOOT\",\
\"CAP_SYS_NICE\",\"CAP_SYS_RESOURCE\",\"CAP_SYS_TIME\",\"CAP_SYS_TTY_CONFIG\",\
\"CAP_MKNOD\",\"CAP_LEASE\",\"CAP_AUDIT_WRITE\",\"CAP_AUDIT_CONTROL\",\
\"CAP_SETFCAP\",\"CAP_MAC_OVERRIDE\",\"CAP_MAC_ADMIN\",\"CAP_SYSLOG\",\
\"CAP_WAKE_ALARM\",\"CAP_BLOCK_SUSPEND\",\"CAP_AUDIT_READ\",\"CAP_PERFMON\",\
\"CAP_BPF\",\"CAP_CHECKPOINT_RESTORE\"]"
#-----------------------------------------------------------------------------
for i in ${CRUN} ${PTYS} ${XAUTH} ${GREP} ${PATCHELF} ; do
  if [ ! -x ${i} ]; then 
    echo NOT FOUND:
    echo ${i}
    echo
    exit
  fi
done
#-----------------------------------------------------------------------------
${PATCHELF} --force-rpath --set-rpath ${LIBCDIR} ${CRUN}
${PATCHELF} --force-rpath --set-rpath ${LIBCDIR} ${PTYS}
${PATCHELF} --set-interpreter ${LIBCDIR}/ld-linux-x86-64.so.2 ${CRUN}
${PATCHELF} --set-interpreter ${LIBCDIR}/ld-linux-x86-64.so.2 ${PTYS}
#-----------------------------------------------------------------------------
XAUTH_LINE=$(${XAUTH} list |grep unix${DISPLAY})
XAUTH_CODE=${XAUTH_LINE##*MIT-MAGIC-COOKIE-1 }
cp -f ./demos/* ${ROOTFS}/root
#---------------------------------------------------------------------------
cat > ${ROOTFS}/usr/bin/init_starter_crun << EOF
#!/bin/bash
export TERM=tmux-direct
export XAUTHORITY="/var/lib/cloonix/cache/.Xauthority"
PTYS="/usr/libexec/cloonix/server/cloonix-scriptpty"
\${PTYS} rsyslogd
\${PTYS} "localedef -i en_US -f UTF-8 en_US.UTF-8"
\${PTYS} "touch /var/lib/cloonix/cache/.Xauthority"
\${PTYS} "xauth add crun/unix:0 MIT-MAGIC-COOKIE-1 ${XAUTH_CODE}"
\${PTYS} /bin/bash
EOF
chmod +x ${ROOTFS}/usr/bin/init_starter_crun
#---------------------------------------------------------------------------
LINE=$(${GREP} $USER /etc/passwd | ${GREP} $UID)
cat >> ${ROOTFS}/etc/passwd << EOF
${LINE}
nobody:x:65534:65534:nobody:${HOME}:/bin/bash
EOF
#---------------------------------------------------------------------------
LINE=$(${GREP} $USER /etc/group | ${GREP} $UID)
cat >> ${ROOTFS}/etc/group << EOF
${LINE}
nogroup:x:65534:
EOF
#-----------------------------------------------------------------------------
sed s"%__STARTER__%init_starter_crun%" ${TEMPLATE} > ${CONFIG}
sed -i s"%__DISPLAY__%${DISPLAY}%" ${CONFIG}
sed -i s"%__ROOTFS__%${ROOTFS}%" ${CONFIG}
sed -i s"%__CAPA__%${CAPA}%"   ${CONFIG}
sed -i s"%__UID__%${UID}%"   ${CONFIG}
sed -i s"%__BULK__%${BULK}%" ${CONFIG}
#-----------------------------------------------------------------------------
mkdir -p ${EXTRACT}/log
#-----------------------------------------------------------------------------
${PTYS} "${CRUN} --rootless=100000 --log=${EXTRACT}/log/crun.log --root=${ROOTFS}/tmp run -f ${CONFIG} nemo"
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------
