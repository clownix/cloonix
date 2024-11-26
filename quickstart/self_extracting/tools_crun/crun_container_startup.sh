#!/bin/bash
#-----------------------------------------------------------------------------
EXTRACT=`pwd`
ROOTFS="${EXTRACT}/rootfs"
CONFIG="${EXTRACT}/config/config.json"
INIT_CNT="${EXTRACT}/config/cloonix-init-starter-crun"
TEMPLATE="${EXTRACT}/config/config.json.template"
CRUN="${EXTRACT}/bin/cloonix-crun"
PTYS="${EXTRACT}/bin/cloonix-scriptpty"
PROXY="${EXTRACT}/bin/cloonix-proxy-crun-access"
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
for i in ${CRUN} ${PTYS} ${PROXY} ${XAUTH} ${GREP} ; do
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
cp -f ${INIT_CNT} ${ROOTFS}/usr/libexec/cloonix/server
sed -i s"/__XAUTH_CODE__/${XAUTH_CODE}/" ${ROOTFS}/usr/libexec/cloonix/server/cloonix-init-starter-crun
#---------------------------------------------------------------------------
PROXYSHARE="/tmp/cloonix_proxy_${RANDOM}"
mkdir -p ${PROXYSHARE}
cp -f ${TEMPLATE} ${CONFIG}
sed -i s"%__PROXYSHARE__%${PROXYSHARE}%" ${CONFIG}
sed -i s"%__DISPLAY__%${DISPLAY}%" ${CONFIG}
sed -i s"%__ROOTFS__%${ROOTFS}%" ${CONFIG}
sed -i s"%__CAPA__%${CAPA}%"   ${CONFIG}
sed -i s"%__UID__%${UID}%"   ${CONFIG}
#-----------------------------------------------------------------------------
mkdir -p ${EXTRACT}/log
#-----------------------------------------------------------------------------
pkill -f cloonix-proxy-crun-access
echo ${PROXY} ${PROXYSHARE}
${PROXY} ${PROXYSHARE}
${PTYS} "${CRUN} --cgroup-manager=disabled --rootless=100000 --log=${EXTRACT}/log/crun.log --root=${ROOTFS}/tmp run -f ${CONFIG} nemo"
#-----------------------------------------------------------------------------
rm -rf ${PROXYSHARE}
echo END SCRIPT
#-----------------------------------------------------------------------------
