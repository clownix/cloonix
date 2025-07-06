#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store
DISTRO="trixie"
REPO="http://127.0.0.1/debian/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
#----------------------------------------------------------------------
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
#----------------------------------------------------------------------
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#----------------------------------------------------------------------
set -e
if [ -e ${WORK} ]; then
  unshare --map-root-user --user --map-auto --mount -- /bin/bash -c "chown -R root:root ${WORK}"
  rm -rf ${WORK}
fi
mkdir -vp ${WORK}
mkdir -p ${TARGZ}
cd ${WORK}
set +e
#----------------------------------------------------------------------
LIST="systemd,rsyslog,bash-completion,vim,xauth,locales,"
LIST+="x11-xkb-utils,xorriso,procps,x11vnc,fuse-zip,"
LIST+="x11-xserver-utils,libcap2-bin,net-tools,strace,"
LIST+="iproute2,x11-apps,unzip,acl,less,lsof,iputils-ping,"
LIST+="traceroute,iputils-tracepath"
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} \
                      --variant=minbase \
                      --include=${LIST} \
                      ${DISTRO} \
                      ${WORK}/trixie_base_rootfs ${REPO}"
#----------------------------------------------------------------------
set -e
cd ${WORK}

#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/etc/systemd"
DIR_DST="base_rootfs/etc/systemd"
mkdir -p ${DIR_DST}/system/multi-user.target.wants
for i in "system.conf" "journald.conf"; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/lib/x86_64-linux-gnu/systemd"
DIR_DST="base_rootfs/lib/x86_64-linux-gnu/systemd"
mkdir -p ${DIR_DST}
for i in "libsystemd-shared-257.so" "libsystemd-core-257.so" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/lib/systemd"
DIR_DST="base_rootfs/lib/systemd"
mkdir -p ${DIR_DST}/journald.conf.d
for i in "systemd" "systemd-journald" "systemd-executor" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
cp ${DIR_SRC}/journald.conf.d/syslog.conf ${DIR_DST}/journald.conf.d
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/lib/systemd/system"
DIR_DST="base_rootfs/lib/systemd/system"
mkdir -p ${DIR_DST}/sockets.target.wants
for i in "basic.target" "graphical.target" "multi-user.target" \
         "rsyslog.service" "sockets.target" "sysinit.target" \
         "syslog.socket" "systemd-initctl.service" \
         "systemd-initctl.socket" "systemd-journald.service" \
         "systemd-journald.socket" "timers.target" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/usr/lib/x86_64-linux-gnu/rsyslog"
DIR_DST="base_rootfs/lib/x86_64-linux-gnu/rsyslog"
mkdir -p ${DIR_DST}
for i in "lmnet.so" "imklog.so" "imuxsock.so" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------

#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/usr/sbin"
DIR_DST="base_rootfs/sbin"
mkdir -p ${DIR_DST}
for i in "locale-gen" "sysctl" "rtacct" "capsh" "nologin" "usermod" \
         "start-stop-daemon" "groupadd" "agetty" "update-passwd" "mkfs"\
         "service" "useradd" "setcap" "rsyslogd" "userdel" "groupdel" \
         "chroot" "groupmod" "getcap" "losetup" "deluser" "adduser" \
         "mount.fuse3" "ip" "ifconfig" "arp" "route" "ipmaddr" \
         "iptunnel" "traceroute" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/usr/bin"
DIR_DST="base_rootfs/bin"
mkdir -p ${DIR_DST}
cp -f 'trixie_base_rootfs/usr/bin/[' ${DIR_DST}
cp -f ${DIR_SRC}/which.debianutils  ${DIR_DST}/which
cp -f ${DIR_SRC}/fuse-zip ${DIR_DST}/cloonix-fuse-zip
cp -f ${DIR_SRC}/x11vnc ${DIR_DST}/cloonix-novnc-x11vnc
cp -f ${DIR_SRC}/xsetroot ${DIR_DST}/cloonix-novnc-xsetroot
for i in "mawk" "bash" "cat" "chmod" "chown" "clear" "cmp" "cp" \
         "cut" "dash" "date" "dd" "df" "diff" "diff3" "dir" \
         "dirname" "du" "echo" "egrep" "env" "expr" "false" \
         "fgrep" "find" "findmnt" "fmt" "free" "funzip" \
         "fusermount3" "getconf" "getent" "getfacl" "getopt" \
         "gpasswd" "grep" "groups" "gunzip" "gzip" "hexdump" \
         "hostid" "hostname" "id" "journalctl" "kill" "ldd" \
         "less" "link" "ln" "locale" "localectl" "localedef" \
         "logger" "login" "ls" "lsblk" "lsmem" "lsns" "lsof" \
         "md5sum" "mkdir" "mkfifo" "mknod" "mktemp" "more" \
         "mount" "mountpoint" "mv" "netstat" "nohup" "nice" \
         "nproc" "nsenter" "passwd" "paste" "pgrep" "pidwait" \
         "printenv" "printf" "ps" "pwd" "rm" "rmdir" "script" \
         "sed" "setfacl" "sleep" "sort" "split" "ss" "stat" \
         "strace" "stty" "su" "sync" "systemctl" "systemd*" \
         "tail" "tar" "tee" "test" "top" "touch" "true" \
         "truncate" "tset" "tsort" "tty" "umount" "uname" \
         "unlink" "unshare" "unzip" "users" "wall" "watch" \
         "wc" "whatis" "whereis" "who" "whoami" "vim.basic" \
         "xargs" "xauth" "xcalc" "xclock" "xeyes" "xhost" \
         "xorriso" "xset" "yes" "zcat" "ping" "xkbcomp" "perl" \
         "tracepath" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
mkdir -p base_rootfs/lib/locale
cp -Lrf trixie_base_rootfs/usr/lib/locale/C.utf8 base_rootfs/lib/locale
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/usr/share/bash-completion/completions"
DIR_DST="base_rootfs/share/bash-completion/completions"
mkdir -p ${DIR_DST}
for i in "sysctl" "usermod" "lsof" "findmnt" "ps" "ss" "useradd" \
         "groupadd" "systemctl" "losetup" "groupdel" "mount" \
         "locale-gen" "strace" "strings" "tar" "userdel" "nsenter" \
         "hexdump" "find" "watch" "systemd-run" "umount" \
         "systemd-analyze" "lsns" "pidof" "pgrep" "pwd"; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
DIR_SRC="trixie_base_rootfs/usr/share/bash-completion"
DIR_DST="base_rootfs/share/bash-completion"
cp ${DIR_SRC}/bash_completion ${DIR_DST}
#----------------------------------------------------------------------
DIR_SRC="trixie_base_rootfs/usr/lib/x86_64-linux-gnu"
DIR_DST="base_rootfs/lib64"
mkdir -p ${DIR_DST}
cp ${DIR_SRC}/ld-linux-x86-64.so.2 ${DIR_DST}
#----------------------------------------------------------------------
cat > base_rootfs/etc/rsyslog.conf << "EOF"
module(load="imuxsock") 
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
EOF
#----------------------------------------------------------------------
cat > base_rootfs/lib/systemd/system/rsyslog.service << "EOF"
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
#----------------------------------------------------------------------
cat > base_rootfs/etc/systemd/system/cloonix_server.service << "EOF"
[Unit]
  Description=cloonix_server
  After=rsyslog
  ConditionPathExists=/cloonix/cloonfs/etc/systemd/system/cloonix_server.service

[Service]
  Type=forking
  ExecStart=/usr/bin/cloonix_net __IDENT__

[Install]
  WantedBy=multi-user.target
EOF
#-----------------------------------------------------------------------
cat > base_rootfs/sbin/cloonix_agent.sh << "EOF"
#!/bin/bash
/cloonixmnt/cnf_fs/cloonix-agent
/cloonixmnt/cnf_fs/cloonix-dropbear-sshd
if [ -e /cloonixmnt/cnf_fs/startup_nb_eth ]; then
  NB_ETH=$(cat /cloonixmnt/cnf_fs/startup_nb_eth)
  NB_ETH=$((NB_ETH-1))
  for i in $(seq 0 $NB_ETH); do
    LIST="${LIST} eth$i"
  done
  for eth in $LIST; do
    VAL=$(ip link show | grep ": ${eth}@")
    while [  $(ip link show | egrep -c ": ${eth}") = 0 ]; do
      sleep 1
    done
  done
fi
EOF
#-----------------------------------------------------------------------
chmod +x base_rootfs/sbin/cloonix_agent.sh
#-----------------------------------------------------------------------
cat > base_rootfs/etc/systemd/system/cloonix_agent.service << EOF
[Unit]
  Description=cloonix_agent
  After=rsyslog.service
  ConditionPathExists=!/cloonix/cloonfs/etc/systemd/system/cloonix_server.service

[Service]
  TimeoutStartSec=infinity
  Type=forking
  RemainAfterExit=y
  ExecStart=/sbin/cloonix_agent.sh

[Install]
  WantedBy=multi-user.target
EOF
#-----------------------------------------------------------------------
cat > base_rootfs/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
EOF
#----------------------------------------------------------------------
cat > base_rootfs/etc/group << "EOF"
root:x:0:
audio:x:29:pulse
nogroup:x:65534:
EOF
#----------------------------------------------------------------------
cat > base_rootfs/etc/locale.gen << "EOF"
en_US.UTF-8 UTF-8
EOF
#----------------------------------------------------------------------
cat > base_rootfs/etc/locale.conf << "EOF"
LANG=C
EOF
#----------------------------------------------------------------------
cat > base_rootfs/etc/hosts << "EOF"
127.0.0.1   localhost
EOF
#----------------------------------------------------------------------
cd ${WORK}/base_rootfs/bin
ln -s bash sh
ln -s mawk awk
ln -s mawk nawk
ln -s vim.basic vi
ln -s vim.basic vim
ln -s vim.basic vimdiff
#----------------------------------------------------------------------
cd ${WORK}/base_rootfs/sbin
ln -s mount.fuse3 mount.fuse
ln -s adduser addgroup
ln -s deluser delgroup
#----------------------------------------------------------------------
cd ${WORK}/base_rootfs/etc/systemd/system/multi-user.target.wants
ln -s /lib/systemd/system/rsyslog.service rsyslog.service
cd ${WORK}/base_rootfs/etc/systemd/system
ln -s /lib/systemd/system/graphical.target default.target
cd ${WORK}/base_rootfs/lib/systemd/system/sockets.target.wants
ln -s /lib/systemd/system/systemd-journald.socket systemd-journald.socket
#----------------------------------------------------------------------
mkdir -p ${WORK}/base_rootfs/usr/local
cd ${WORK}/base_rootfs/usr
for i in "bin" "sbin" "lib" "lib64" "share" ; do
  ln -s ../${i} ${i}
done
cd ${WORK}/base_rootfs/usr/local
for i in "bin" "sbin" "lib" ; do
  ln -s ../../${i} ${i}
done
#----------------------------------------------------------------------
cd ${WORK}/base_rootfs
unshare -rm tar zcvf ${TARGZ}/trixie_base_rootfs.tar.gz .
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c "chown -R root:root ${WORK}"
rm -rf ${WORK}
#----------------------------------------------------------------------

