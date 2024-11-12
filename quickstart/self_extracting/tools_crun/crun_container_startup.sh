#!/bin/bash
#-----------------------------------------------------------------------------
EXTRACT=`pwd`
ROOTFS="${EXTRACT}/rootfs"
CONFIG="${EXTRACT}/config/config.json"
TEMPLATE="${EXTRACT}/config/config.json.template"
CRUN="${EXTRACT}/bin/cloonix-crun"
PTYS="${EXTRACT}/bin/cloonix-scriptpty"
XAUTH="${EXTRACT}/bin/xauth"
GREP="${ROOTFS}/usr/libexec/cloonix/common/grep"
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
for i in ${CRUN} ${PTYS} ${XAUTH} ${GREP} ; do
  if [ ! -x ${i} ]; then 
    echo NOT FOUND:
    echo ${i}
    echo
    exit
  fi
done
#-----------------------------------------------------------------------------
XAUTH_LINE=$(${XAUTH} list |grep unix${DISPLAY})
XAUTH_CODE=${XAUTH_LINE##*MIT-MAGIC-COOKIE-1 }
#---------------------------------------------------------------------------
cat > ${ROOTFS}/usr/libexec/cloonix/server/init_starter_crun << EOF
#!/usr/libexec/cloonix/common/bash
export TERM=linux
export XAUTHORITY="/var/lib/cloonix/cache/.Xauthority"

/usr/libexec/cloonix/common/ln -s /usr/libexec/cloonix/common/share /usr/share
/usr/libexec/cloonix/common/ln -s /usr/libexec/cloonix/common/lib /usr/lib
/usr/libexec/cloonix/common/ln -s /usr/libexec/cloonix/common /usr/bin
/usr/libexec/cloonix/common/ln -s /usr/libexec/cloonix/common/etc /etc

/usr/libexec/cloonix/server/cloonix-scriptpty rsyslogd
/usr/libexec/cloonix/server/cloonix-scriptpty "localedef -i en_US -f UTF-8 en_US.UTF-8"
/usr/libexec/cloonix/server/cloonix-scriptpty "touch /var/lib/cloonix/cache/.Xauthority"
/usr/libexec/cloonix/server/cloonix-scriptpty "xauth add crun/unix:0 MIT-MAGIC-COOKIE-1 ${XAUTH_CODE}"
/usr/libexec/cloonix/server/cloonix-scriptpty bash
EOF
chmod +x ${ROOTFS}/usr/libexec/cloonix/server/init_starter_crun
#---------------------------------------------------------------------------
cp -f ${TEMPLATE} ${CONFIG}
sed -i s"%__DISPLAY__%${DISPLAY}%" ${CONFIG}
sed -i s"%__ROOTFS__%${ROOTFS}%" ${CONFIG}
sed -i s"%__CAPA__%${CAPA}%"   ${CONFIG}
sed -i s"%__UID__%${UID}%"   ${CONFIG}
#-----------------------------------------------------------------------------
mkdir -p ${EXTRACT}/log
#-----------------------------------------------------------------------------
${PTYS} "${CRUN} --cgroup-manager=disabled --rootless=100000 --log=${EXTRACT}/log/crun.log --root=${ROOTFS}/tmp run -f ${CONFIG} nemo"
#-----------------------------------------------------------------------------
echo END SCRIPT
#-----------------------------------------------------------------------------
