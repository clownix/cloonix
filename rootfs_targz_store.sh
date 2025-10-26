#!/bin/bash
HERE=`pwd`
DISTRO="trixie"
REPO="http://127.0.0.1/debian/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
TARGZ="${HERE}/targz_store"
WORK="${HERE}/work_targz_store"
SRC_ROOTFS_NAME="trixie_base_rootfs"
SRC_ROOTFS="${WORK}/${SRC_ROOTFS_NAME}"
DST_ROOTFS="${SRC_ROOTFS}/target_rootfs"
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
#----------------------------------------------------------------------#
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${SRC_ROOTFS}" \
        -- /bin/bash -c \
        "touch dev/zero; touch dev/null ; \
         mount -o rw,bind /dev/zero dev/zero ; \
         mount -o rw,bind /dev/null dev/null ; \
         /sbin/chroot . /bin/sh -c '$1'"
}
#----------------------------------------------------------------------#
cd ${WORK}
set +e
#----------------------------------------------------------------------
LIST="systemd,rsyslog,bash-completion,vim,xauth,locales,"
LIST+="x11-xkb-utils,xorriso,procps,x11vnc,fuse-zip,"
LIST+="x11-xserver-utils,libcap2-bin,net-tools,strace,"
LIST+="iproute2,x11-apps,unzip,acl,less,lsof,iputils-ping,"
LIST+="iputils-tracepath,libglib-perl,xkb-data,iso-codes,"
LIST+="libqt6gui6,libgtk-3-common,libgtk-3-0t64,"
LIST+="shared-mime-info,gcc,libc6-dev,libgstreamer1.0-0,"
LIST+="libpipewire-0.3-modules,gstreamer1.0-plugins-base,"
LIST+="gstreamer1.0-plugins-good,kmod,"
LIST+="hostapd,wpasupplicant,iw,tcpdump,"
LIST+="python3,python3-websockify"

unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} \
                      --variant=minbase \
                      --include=${LIST} \
                      ${DISTRO} \
                      ${SRC_ROOTFS} ${REPO}"
#----------------------------------------------------------------------
set -e
mkdir -p ${SRC_ROOTFS}/root
cp -f ${HERE}/build_tools/toolbox/fill_common.c ${SRC_ROOTFS}/root
fct_unshare_chroot "gcc -o /root/copy_libs /root/fill_common.c"
fct_unshare_chroot "update-mime-database /usr/share/mime"
#----------------------------------------------------------------------
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
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/lib/x86_64-linux-gnu/systemd"
DIR_DST="${DST_ROOTFS}/lib/x86_64-linux-gnu/systemd"
mkdir -p ${DIR_DST}
for i in "libsystemd-shared-257.so" "libsystemd-core-257.so" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/lib/systemd"
DIR_DST="${DST_ROOTFS}/lib/systemd"
mkdir -p ${DIR_DST}/journald.conf.d
for i in "systemd" "systemd-journald" "systemd-executor" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
cp ${DIR_SRC}/journald.conf.d/syslog.conf ${DIR_DST}/journald.conf.d
#----------------------------------------------------------------------
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
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/lib/x86_64-linux-gnu/rsyslog"
DIR_DST="${DST_ROOTFS}/lib/x86_64-linux-gnu/rsyslog"
mkdir -p ${DIR_DST}
for i in "lmnet.so" "imklog.so" "imuxsock.so" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------

#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/sbin"
DIR_DST="${DST_ROOTFS}/sbin"
mkdir -p ${DIR_DST}
for i in "locale-gen" "sysctl" "rtacct" "capsh" "nologin" "usermod" \
         "start-stop-daemon" "groupadd" "agetty" "update-passwd" "mkfs"\
         "service" "useradd" "setcap" "rsyslogd" "userdel" "groupdel" \
         "chroot" "groupmod" "getcap" "losetup" "deluser" "adduser" \
         "mount.fuse3" "ip" "ifconfig" "arp" "route" "ipmaddr" "iptunnel" \
         "lsmod" "modprobe" "insmod" "rmmod" "modinfo" "depmod" "hostapd" \
         "hostapd_cli" "wpa_action" "wpa_cli" "wpa_supplicant" \
         "iw" ; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done

#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/bin"
DIR_DST="${DST_ROOTFS}/bin"
mkdir -p ${DIR_DST}
cp -f 'trixie_base_rootfs/usr/bin/[' ${DIR_DST}
cp -f ${DIR_SRC}/which.debianutils  ${DIR_DST}/which
cp -f ${DIR_SRC}/websockify ${DIR_DST}/cloonix-novnc-websockify
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
         "tracepath" "tcpdump" "websockify"; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/lib/locale"
DIR_DST="${DST_ROOTFS}/lib/locale"
mkdir -p ${DIR_DST}
cp -Lrf ${DIR_SRC}/C.utf8 ${DIR_DST}
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/share/bash-completion/completions"
DIR_DST="${DST_ROOTFS}/share/bash-completion/completions"
mkdir -p ${DIR_DST}
for i in "sysctl" "usermod" "lsof" "findmnt" "ps" "ss" "useradd" \
         "groupadd" "systemctl" "losetup" "groupdel" "mount" \
         "locale-gen" "strace" "strings" "tar" "userdel" "nsenter" \
         "hexdump" "find" "watch" "systemd-run" "umount" \
         "systemd-analyze" "lsns" "pidof" "pgrep" "pwd"; do
  cp ${DIR_SRC}/${i} ${DIR_DST}
done
DIR_SRC="${SRC_ROOTFS}/usr/share/bash-completion"
DIR_DST="${DST_ROOTFS}/share/bash-completion"
cp ${DIR_SRC}/bash_completion ${DIR_DST}
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr/lib/x86_64-linux-gnu"
DIR_DST="${DST_ROOTFS}/lib64"
mkdir -p ${DIR_DST}
cp ${DIR_SRC}/ld-linux-x86-64.so.2 ${DIR_DST}
#----------------------------------------------------------------------
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
#----------------------------------------------------------------------
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
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc/systemd/system"
cat > ${DIR_DST}/cloonix_server.service << "EOF"
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
DIR_DST="${DST_ROOTFS}/sbin"
cat > ${DIR_DST}/cloonix_agent.sh << "EOF"
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
chmod +x ${DIR_DST}/cloonix_agent.sh
#-----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc/systemd/system"
cat > ${DIR_DST}/cloonix_agent.service << EOF
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
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/group << "EOF"
root:x:0:
audio:x:29:pulse
nogroup:x:65534:
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/locale.gen << "EOF"
en_US.UTF-8 UTF-8
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/locale.conf << "EOF"
LANG=C
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc"
cat > ${DIR_DST}/hosts << "EOF"
127.0.0.1   localhost
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/bin"
cd ${DIR_DST}
ln -s bash sh
ln -s mawk awk
ln -s mawk nawk
ln -s vim.basic vi
ln -s vim.basic vim
ln -s vim.basic vimdiff
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/sbin"
cd ${DIR_DST}
ln -s mount.fuse3 mount.fuse
ln -s adduser addgroup
ln -s deluser delgroup
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc/systemd/system/multi-user.target.wants"
cd ${DIR_DST}
ln -s /lib/systemd/system/rsyslog.service rsyslog.service
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc/systemd/system"
cd ${DIR_DST}
ln -s /lib/systemd/system/graphical.target default.target
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/lib/systemd/system/sockets.target.wants"
cd ${DIR_DST}
ln -s /lib/systemd/system/systemd-journald.socket systemd-journald.socket
#----------------------------------------------------------------------
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
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/var"
for i in "log" "spool/rsyslog" "lib/cloonix/cache" \
         "lib/cloonix/bulk" "lib/cloonix/cache/fontconfig" ; do
  mkdir -p ${DIR_DST}/${i}
done
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/share"
for i in "fonts/truetype" "terminfo" "mime" "misc" "dconf" \
         "locale" "i18n/charmaps" "i18n/locales" "X11/locale" \
         "glib-2.0/schemas" "vim/vim91" ; do
  mkdir -p ${DIR_DST}/${i}
done
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}"
for i in "dev/net" "lib/modules" "bin/ovsschema" \
         "lib/x86_64-linux-gnu/qt6/plugins/platforms" \
         "lib/x86_64-linux-gnu/gstreamer-1.0" \
         "lib/x86_64-linux-gnu/gconv"; do
  mkdir -p ${DIR_DST}/${i}
done
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/etc/fonts"
mkdir -p ${DIR_DST}
cat > ${DIR_DST}/fonts.conf << EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
    <dir>/usr/libexec/cloonix/cloonfs/share</dir>
    <cachedir>/var/lib/cloonix/cache/fontconfig</cachedir>
    <cachedir prefix="xdg">fontconfig</cachedir>
</fontconfig>
EOF
#----------------------------------------------------------------------
DIR_DST="${DST_ROOTFS}/share/vim/vim91"
touch ${DIR_DST}/defaults.vim
#----------------------------------------------------------------------
DIR_SRC="${SRC_ROOTFS}/usr"
DIR_DST="${DST_ROOTFS}"
cp -Lrf ${DIR_SRC}/lib/x86_64-linux-gnu/perl-base ${DIR_DST}/lib/x86_64-linux-gnu
cp -Lrf ${DIR_SRC}/lib/x86_64-linux-gnu/perl5 ${DIR_DST}/lib/x86_64-linux-gnu
cp -Lrf ${DIR_SRC}/share/perl5  ${DIR_DST}/share
cp -Lrf ${DIR_SRC}/lib/locale/* ${DIR_DST}/lib/locale
cp -Lrf ${DIR_SRC}/share/terminfo/l ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/terminfo/t ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/terminfo/x ${DIR_DST}/share/terminfo
cp -Lrf ${DIR_SRC}/share/fonts/truetype/dejavu ${DIR_DST}/share/fonts/truetype
cp -Lrf ${DIR_SRC}/share/fontconfig ${DIR_DST}/share
cp -Lrf ${DIR_SRC}/share/X11/locale/en_US.UTF-8 ${DIR_DST}/share/X11/locale
cp -Lrf ${DIR_SRC}/share/X11/locale/C ${DIR_DST}/share/X11/locale
cp -Lrf ${DIR_SRC}/share/X11/locale/locale.dir ${DIR_DST}/share/X11/locale
cp -Lrf ${DIR_SRC}/share/X11/xkb ${DIR_DST}/share/X11
cp  -f  ${DIR_SRC}/share/X11/rgb.txt ${DIR_DST}/share/X11
#----------------------------------------------------------------------
cp -Lrf ${DIR_SRC}/bin/python3* ${DIR_DST}/bin
cp -Lrf ${DIR_SRC}/lib/python3* ${DIR_DST}/lib
#----------------------------------------------------------------------
cp -f   ${DIR_SRC}/lib/x86_64-linux-gnu/qt6/plugins/platforms/libqxcb.so ${DIR_DST}/lib/x86_64-linux-gnu/qt6/plugins/platforms
cp -Lrf ${DIR_SRC}/lib/x86_64-linux-gnu/gtk-3.0 ${DIR_DST}/lib/x86_64-linux-gnu
cp -Lrf ${DIR_SRC}/share/themes ${DIR_DST}/share
cp -f   ${DIR_SRC}/share/glib-2.0/schemas/gschemas.compiled ${DIR_DST}/share/glib-2.0/schemas
cp -f   ${DIR_SRC}/share/mime/mime.cache ${DIR_DST}/share/mime
cp -f   ${DIR_SRC}/lib/x86_64-linux-gnu/gconv/gconv-modules.cache ${DIR_DST}/lib/x86_64-linux-gnu/gconv
cp -Lrf ${DIR_SRC}/share/gstreamer-1.0 ${DIR_DST}/share
cp -Lrf ${DIR_SRC}/lib/x86_64-linux-gnu/gstreamer1.0 ${DIR_DST}/lib/x86_64-linux-gnu 
cp -Lrf ${DIR_SRC}/lib/x86_64-linux-gnu/pipewire-0.3 ${DIR_DST}/lib/x86_64-linux-gnu
LONG_DIR_SRC="${DIR_SRC}/lib/x86_64-linux-gnu/gstreamer-1.0"
LONG_DIR_DST="${DIR_DST}/lib/x86_64-linux-gnu/gstreamer-1.0"
for i in "libgstpulseaudio.so" \
         "libgstautodetect.so" \
         "libgstaudioresample.so" \
         "libgstaudioconvert.so" \
         "libgstcoreelements.so" \
         "libgstapp.so" ; do
  cp -rf ${LONG_DIR_SRC}/${i} ${LONG_DIR_DST}
done
#----------------------------------------------------------------------
for i in "en_GB" "en" "locale.alias" ; do
  cp -Lrf ${DIR_SRC}/share/locale/${i} ${DIR_DST}/share/locale
done
cp -f ${DIR_SRC}/share/i18n/charmaps/UTF-8.gz ${DIR_DST}/share/i18n/charmaps
for i in "en_US" "en_GB" "i18n" "i18n_ctype" "iso14651_t1" \
         "iso14651_t1_common" "translit_*" ; do
  cp -f ${DIR_SRC}/share/i18n/locales/${i} ${DIR_DST}/share/i18n/locales
done
cd ${DIR_DST}/share/i18n/charmaps
gunzip -f UTF-8.gz
cd ${WORK}
#----------------------------------------------------------------------
tar --directory=${DIR_DST}/share -xf ${TARGZ}/noVNC_*.tar.gz
#----------------------------------------------------------------------
openssl req -x509 -nodes -newkey rsa:3072 -keyout novnc.pem -subj \
            "/CN=cloonix.net" -out ${DIR_DST}/bin/novnc.pem -days 3650
rm -f novnc.pem
#----------------------------------------------------------------------
fct_unshare_chroot "/root/copy_libs /target_rootfs"
#----------------------------------------------------------------------
cd ${DST_ROOTFS}
unshare -rm tar zcvf ${TARGZ}/${SRC_ROOTFS_NAME}.tar.gz .
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c "chown -R root:root ${WORK}"
rm -rf ${WORK}
#----------------------------------------------------------------------

