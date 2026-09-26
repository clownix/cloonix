#!/bin/bash
HERE=`pwd`
DISTRO="trixie"
REPO="http://127.0.0.1/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
TARGZ="${HERE}/targz_store"
WORK="${HERE}/work_targz_store"
SRC_ROOTFS_NAME="trixie_base_rootfs"
SRC_ROOTFS="${WORK}/${SRC_ROOTFS_NAME}"
DST_ROOTFS="${SRC_ROOTFS}/target_rootfs"
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
set -e
if [ -e ${WORK} ]; then
  unshare --map-root-user --user --map-auto --mount -- /bin/bash -c "chown -R root:root ${WORK}"
  rm -rf ${WORK}
fi
mkdir -vp ${WORK}
mkdir -p ${TARGZ}
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${SRC_ROOTFS}" \
        -- /bin/bash -c \
        "touch dev/zero; touch dev/null ; \
         mount -o rw,bind /dev/zero dev/zero ; \
         mount -o rw,bind /dev/null dev/null ; \
         /sbin/chroot . /bin/sh -c '$1'"
}
cd ${WORK}
set +e
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
WITH_KMOD=0
WITH_SYSTEMD=0
WITH_MD5SUM=0

LIST="adduser,rsyslog,bash-completion,vim-tiny,locales,"
LIST+="xorriso,procps,"
LIST+="libcap2-bin,net-tools,"
LIST+="iproute2,unzip,acl,less,lsof,iputils-ping,"
LIST+="iputils-tracepath,"
LIST+="gcc,libc6-dev,"
LIST+="tcpdump,iw"
[ ${WITH_SYSTEMD} -eq 1 ] && LIST+=",systemd"
[ ${WITH_KMOD}    -eq 1 ] && LIST+=",kmod"

unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} \
                      --variant=minbase \
                      --include=${LIST} \
                      ${DISTRO} \
                      ${SRC_ROOTFS} ${REPO}"
set -e
mkdir -p ${SRC_ROOTFS}/root
cp -f ${HERE}/build_tools/toolbox/fill_common.c ${SRC_ROOTFS}/root
fct_unshare_chroot "gcc -o /root/copy_libs /root/fill_common.c"
# The skeleton of the target, made here and on purpose.
mkdir -p ${DST_ROOTFS}/etc
# Everything systemd, in one block so WITH_SYSTEMD=0 takes all of it and not
# half.
if [ ${WITH_SYSTEMD} -eq 1 ]; then
  DIR_SRC="${SRC_ROOTFS}/etc/systemd"
  DIR_DST="${DST_ROOTFS}/etc/systemd"
  mkdir -p ${DIR_DST}/system/multi-user.target.wants
  for i in "system.conf" "journald.conf"; do
    cp ${DIR_SRC}/${i} ${DIR_DST}
  done
  sed -i s"/#DefaultCPUAccounting=yes/DefaultCPUAccounting=no/" ${DIR_DST}/system.conf
  sed -i s"/#DefaultMemoryAccounting=yes/DefaultMemoryAccounting=no/" ${DIR_DST}/system.conf
  sed -i s"/#DefaultTasksAccounting=yes/DefaultTasksAccounting=no/" ${DIR_DST}/system.conf
  echo "DefaultControllers=" >> ${DIR_DST}/system.conf
  DIR_SRC="${SRC_ROOTFS}/lib/x86_64-linux-gnu/systemd"
  DIR_DST="${DST_ROOTFS}/lib/x86_64-linux-gnu/systemd"
  mkdir -p ${DIR_DST}
  for i in "libsystemd-shared-257.so" "libsystemd-core-257.so" ; do
    cp ${DIR_SRC}/${i} ${DIR_DST}
  done
  DIR_SRC="${SRC_ROOTFS}/lib/systemd"
  DIR_DST="${DST_ROOTFS}/lib/systemd"
  mkdir -p ${DIR_DST}/journald.conf.d
  for i in "systemd" "systemd-journald" "systemd-executor" ; do
    cp ${DIR_SRC}/${i} ${DIR_DST}
  done
  cp ${DIR_SRC}/journald.conf.d/syslog.conf ${DIR_DST}/journald.conf.d
  DIR_SRC="${SRC_ROOTFS}/lib/systemd/system"
  DIR_DST="${DST_ROOTFS}/lib/systemd/system"
  mkdir -p ${DIR_DST}/sockets.target.wants
  for i in "basic.target" "graphical.target" "multi-user.target" \
           "rsyslog.service" "sockets.target" "sysinit.target" \
           "syslog.socket" "systemd-initctl.service" \
           "systemd-initctl.socket" "systemd-journald.service" \
           "systemd-journald.socket" "timers.target" ; do
    cp ${DIR_SRC}/${i} ${DIR_DST}
  done
fi
DIR_SRC="${SRC_ROOTFS}/usr/lib/x86_64-linux-gnu/rsyslog"
DIR_DST="${DST_ROOTFS}/lib/x86_64-linux-gnu/rsyslog"
mkdir -p ${DIR_DST}
for i in "lmnet.so" "imklog.so" "imuxsock.so" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done

DIR_SRC="${SRC_ROOTFS}/usr/sbin"
DIR_DST="${DST_ROOTFS}/sbin"
mkdir -p ${DIR_DST}
SBIN_LIST="locale-gen sysctl rtacct capsh nologin usermod \
           start-stop-daemon groupadd agetty update-passwd mkfs \
           service useradd setcap rsyslogd userdel groupdel \
           chroot groupmod getcap losetup \
           ip ifconfig arp route ipmaddr iptunnel"
# Each of these pulls libcrypto in; see the switches at the top.
[ ${WITH_KMOD} -eq 1 ] && \
  SBIN_LIST+=" lsmod modprobe insmod rmmod modinfo depmod"
SBIN_LIST+=" iw"
for i in ${SBIN_LIST}; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done

DIR_SRC="${SRC_ROOTFS}/usr/bin"
DIR_DST="${DST_ROOTFS}/bin"
mkdir -p ${DIR_DST}
cp -f 'trixie_base_rootfs/usr/bin/[' ${DIR_DST}
cp -f ${DIR_SRC}/which.debianutils  ${DIR_DST}/which
for i in "mawk" "bash" "cat" "chmod" "chown" "clear" "cmp" "cp" \
         "cut" "dash" "date" "dd" "df" "diff" "diff3" "dir" \
         "dirname" "du" "echo" "egrep" "env" "expr" "false" \
         "fgrep" "find" "findmnt" "fmt" "free" "funzip" \
         "getconf" "getent" "getfacl" "getopt" "gunzip" "gzip" \
         "gpasswd" "grep" "groups" "xargs" "wc" "vim.tiny" \
         "hostid" "hostname" "id" "kill" "ldd" \
         "less" "link" "ln" "locale" "localedef" \
         "logger" "login" "ls" "lsblk" "lsmem" "lsns" "lsof" \
         "mkdir" "mkfifo" "mknod" "mktemp" "more" \
         "mount" "mountpoint" "mv" "netstat" "nohup" "nice" \
         "nproc" "nsenter" "passwd" "paste" "pgrep" "pidwait" \
         "printenv" "printf" "ps" "pwd" "rm" "rmdir" "script" \
         "sed" "setfacl" "sleep" "sort" "split" "ss" "stat" \
         "stty" "su" "sync" "tail" \
         "tar" "tee" "test" "top" "touch" "true" "truncate" \
         "tset" "tsort" "tty" "umount" "uname" "unlink" \
         "unshare" "unzip" "users" "wall" "watch" "xorriso" \
         "yes" "zcat" "ping" "tracepath" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
# journalctl, systemctl, localectl: systemd binaries without systemd in their name.
if [ ${WITH_SYSTEMD} -eq 1 ]; then
  for i in "journalctl" "localectl" "systemctl" "systemd*" ; do
    cp ${DIR_SRC}/${i} ${DIR_DST}
  done
fi
if [ ${WITH_MD5SUM} -eq 1 ]; then
  cp ${DIR_SRC}/md5sum ${DIR_DST}
fi
DIR_SRC="${SRC_ROOTFS}/usr/lib/locale"
DIR_DST="${DST_ROOTFS}/lib/locale"
mkdir -p ${DIR_DST}
cp -Lrf ${DIR_SRC}/C.utf8 ${DIR_DST}
DIR_SRC="${SRC_ROOTFS}/usr/share/bash-completion/completions"
DIR_DST="${DST_ROOTFS}/share/bash-completion/completions"
mkdir -p ${DIR_DST}
COMPL_LIST="sysctl usermod lsof findmnt ps ss useradd \
            groupadd losetup groupdel mount \
            locale-gen strings tar userdel nsenter \
            find watch umount lsns pidof pgrep pwd"
# Completions for binaries that are not in the image are dead files.
[ ${WITH_SYSTEMD} -eq 1 ] && \
  COMPL_LIST+=" systemctl systemd-run systemd-analyze"
for i in ${COMPL_LIST}; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
DIR_SRC="${SRC_ROOTFS}/usr/share/bash-completion"
DIR_DST="${DST_ROOTFS}/share/bash-completion"
cp ${DIR_SRC}/bash_completion ${DIR_DST}
DIR_SRC="${SRC_ROOTFS}/usr/lib/x86_64-linux-gnu"
DIR_DST="${DST_ROOTFS}/lib64"
mkdir -p ${DIR_DST}
cp ${DIR_SRC}/ld-linux-x86-64.so.2 ${DIR_DST}
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/rsyslog.conf << "EOF"
module(load="imuxsock") 
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
EOF
# The two unit files.
if [ ${WITH_SYSTEMD} -eq 1 ]; then
  DIR_DST="${DST_ROOTFS}/lib/systemd/system"
  cat > ${DIR_DST}/rsyslog.service << "EOF"
[Unit]
  Description=System Logging Service
  Requires=syslog.socket

[Service]
  Type=notify
  ExecStart=/usr/sbin/rsyslogd -n -iNONE
  StandardOutput=null
  Restart=on-failure
  LimitNOFILE=16384

[Install]
  WantedBy=multi-user.target
  Alias=syslog.service
EOF
fi

# The accounts the packages created are kept and root is only made sure of.
DIR_DST="${DST_ROOTFS}/etc"
if ! grep -q "^root:" ${DIR_DST}/passwd 2>/dev/null; then
  echo "root:x:0:0:root:/root:/bin/bash" >> ${DIR_DST}/passwd
fi
if ! grep -q "^nobody:" ${DIR_DST}/passwd 2>/dev/null; then
  echo "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin" \
       >> ${DIR_DST}/passwd
fi
DIR_DST="${DST_ROOTFS}/etc"
if ! grep -q "^root:" ${DIR_DST}/group 2>/dev/null; then
  echo "root:x:0:" >> ${DIR_DST}/group
fi
if ! grep -q "^nogroup:" ${DIR_DST}/group 2>/dev/null; then
  echo "nogroup:x:65534:" >> ${DIR_DST}/group
fi
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/locale.gen << "EOF"
EOF
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/locale.conf << "EOF"
LANG=C
EOF
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/hosts << "EOF"
127.0.0.1   localhost
EOF
DIR_DST="${DST_ROOTFS}/bin"
cd ${DIR_DST}
ln -s bash sh
ln -s mawk awk
ln -s mawk nawk
ln -s vim.tiny vi
ln -s vim.tiny vim
if [ ${WITH_SYSTEMD} -eq 1 ]; then
  DIR_DST="${DST_ROOTFS}/etc/systemd/system/multi-user.target.wants"
  cd ${DIR_DST}
  ln -s /lib/systemd/system/rsyslog.service rsyslog.service
  DIR_DST="${DST_ROOTFS}/etc/systemd/system"
  cd ${DIR_DST}
  ln -s /lib/systemd/system/graphical.target default.target
  DIR_DST="${DST_ROOTFS}/lib/systemd/system/sockets.target.wants"
  cd ${DIR_DST}
  ln -s /lib/systemd/system/systemd-journald.socket systemd-journald.socket
fi
DIR_DST="${DST_ROOTFS}/usr"
mkdir -p ${DIR_DST}
cd ${DIR_DST}
for i in "bin" "sbin" "lib" "lib64" "share" ; do
  ln -s ../${i} ${i}
done
mkdir -p ${DIR_DST}/local
cd ${DIR_DST}/local
for i in "bin" "sbin" "lib" ; do
  ln -s ../../${i} ${i}
done
cd ${WORK}
DIR_DST="${DST_ROOTFS}/var"
for i in "log" "spool/rsyslog" "lib/cloonix/cache" \
         "lib/cloonix/bulk" ; do
  mkdir -p ${DIR_DST}/${i}
done
DIR_DST="${DST_ROOTFS}/share"
for i in "terminfo" "locale" ; do
  mkdir -p ${DIR_DST}/${i}
done
DIR_DST="${DST_ROOTFS}"
for i in "dev/net" "bin/ovsschema" "lib/x86_64-linux-gnu/gconv"; do
  mkdir -p ${DIR_DST}/${i}
done
DIR_SRC="${SRC_ROOTFS}/usr"
DIR_DST="${DST_ROOTFS}"
cp -Lrf ${DIR_SRC}/share/terminfo/l ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/terminfo/t ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/terminfo/x ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/locale/locale.alias ${DIR_DST}/share/locale
cd ${WORK}
fct_unshare_chroot "/root/copy_libs /target_rootfs"
DUP_DIR="${DST_ROOTFS}/lib/x86_64-linux-gnu"
for i in "libsystemd-shared-257.so" "libsystemd-core-257.so" ; do
  if [ -f "${DUP_DIR}/systemd/${i}" ] && [ -f "${DUP_DIR}/${i}" ]; then
    rm -f "${DUP_DIR}/${i}"
  fi
done
cd ${DST_ROOTFS}
unshare -rm tar zcvf ${TARGZ}/${SRC_ROOTFS_NAME}.tar.gz .
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c "chown -R root:root ${WORK}"
rm -rf ${WORK}

