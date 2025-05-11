#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
ROOTFS="/root/zipbasic"
#----------------------------------------------------------------------------#
LIST_BIN="/bin/cat /bin/chmod /bin/chown /bin/cp /bin/date /bin/dir \
           /bin/echo /bin/ls /bin/df /bin/mkdir /bin/mknod /bin/mktemp \
           /bin/pwd /bin/rm /bin/rmdir /bin/sleep /bin/sync /bin/touch \
           /bin/mv /bin/true /bin/grep /bin/hostname /bin/kill /bin/ps \
           /bin/mount /bin/umount /bin/netstat /bin/sed /bin/tar /bin/bash \
           /bin/ping /bin/gzip /bin/gunzip /bin/dmesg /bin/lsblk /bin/more \
           /bin/mountpoint /bin/su /bin/ln"
#----------------------------------------------------------------------------#
LIST_SBIN="/sbin/losetup /sbin/ifconfig /sbin/ipmaddr /sbin/rarp \
           /sbin/route /sbin/ip /sbin/blkid /sbin/sysctl" 
#----------------------------------------------------------------------------#
LIST_USBIN="/usr/sbin/rsyslogd /usr/sbin/arping /usr/sbin/chroot \
            /usr/sbin/arp /usr/sbin/locale-gen /usr/sbin/iptables-nft \
            /usr/sbin/nologin /usr/sbin/groupadd /usr/sbin/groupdel \
            /usr/sbin/groupmod /usr/sbin/useradd /usr/sbin/userdel \
            /usr/sbin/usermod /usr/sbin/ldconfig" 
#----------------------------------------------------------------------------#
LIST_UBIN="/usr/bin/iperf3 /usr/bin/strace /usr/sbin/arping /usr/bin/mawk \
           /usr/bin/vim.basic /usr/bin/[ /usr/bin/arch /usr/bin/basename \
           /usr/bin/cut /usr/bin/dirname /usr/bin/env /usr/bin/expand \
           /usr/bin/expr /usr/bin/groups /usr/bin/id /usr/bin/md5sum \
           /usr/bin/nohup /usr/bin/printf /usr/bin/sort /usr/bin/stat \
           /usr/bin/tail /usr/bin/tee /usr/bin/test /usr/bin/timeout \
           /usr/bin/truncate /usr/bin/unlink /usr/bin/wc /usr/bin/who \
           /usr/bin/whoami /usr/bin/yes /usr/bin/cmp /usr/bin/diff \
           /usr/bin/diff3 /usr/bin/sdiff /usr/bin/find /usr/bin/xargs \
           /usr/bin/free /usr/bin/pgrep /usr/bin/pidwait /usr/bin/top \
           /usr/bin/uptime /usr/bin/vmstat /usr/bin/watch /usr/bin/pkill \
           /usr/bin/objdump /usr/bin/readelf /usr/bin/strings /usr/bin/which \
           /usr/bin/file /usr/bin/lsof /usr/bin/ldd /usr/bin/ssh \
           /usr/bin/scp /usr/bin/daemonize /usr/bin/killall /usr/bin/pstree \
           /usr/bin/xeyes /usr/bin/locale /usr/bin/localedef /usr/bin/curl \
           /usr/bin/localectl /usr/bin/xauth /usr/bin/curl /usr/bin/tcpdump \
           /usr/bin/tracepath /usr/bin/wget /usr/bin/script /usr/bin/lsns \
           /usr/bin/nsenter /usr/bin/unshare /usr/bin/passwd \
           /usr/bin/setfacl /usr/bin/getfacl /usr/bin/logger \
           /usr/bin/mtracebis /usr/bin/vtysh"
#----------------------------------------------------------------------------#
for i in ${LIST_BIN} ${LIST_SBIN} ${LIST_UBIN} ${LIST_USBIN}; do
  if [ ! -e "${i}" ]; then
    echo ERROR ${i} not found
    exit 1
  fi
done
#----------------------------------------------------------------------------#
rm -rf ${ROOTFS}
mkdir -p ${ROOTFS}
for i in "bin" "sbin" "root" "etc" "tmp" "lib" "home" "lib64" \
     "var/log" "usr/bin" "usr/sbin" "var/spool/rsyslog" \
     "usr/share/misc" "usr/lib/x86_64-linux-gnu/rsyslog" \
     "usr/share/locale" "lib/x86_64-linux-gnu" "usr/share/X11/locale" \
     "usr/share/bash-completion" "usr/share/bash-completion/completions" \
     "usr/lib/locale" "usr/lib/security" \
     "etc/default" ; do 
  mkdir -v -p ${ROOTFS}/${i}
done
#----------------------------------------------------------------------------#
for i in ${LIST_BIN}; do
  cp -v ${i} ${ROOTFS}/bin
done
for i in ${LIST_SBIN}; do
  cp -v ${i} ${ROOTFS}/sbin
done
for i in ${LIST_UBIN}; do
  cp -v ${i} ${ROOTFS}/usr/bin
done
for i in ${LIST_USBIN}; do
  cp -v ${i} ${ROOTFS}/usr/sbin
done
#----------------------------------------------------------------------------#
cp -Lrf /usr/share/X11/locale/en_US.UTF-8 ${ROOTFS}/usr/share/X11/locale
cp -Lrf /usr/share/X11/locale/iso8859-1 ${ROOTFS}/usr/share/X11/locale
cp -Lrf /usr/share/X11/locale/C ${ROOTFS}/usr/share/X11/locale
cp -Lrf /usr/share/X11/locale/locale.dir ${ROOTFS}/usr/share/X11/locale
cp -Lrf /usr/share/X11/xkb ${ROOTFS}/usr/share/X11
cp -f /usr/share/X11/rgb.txt ${ROOTFS}/usr/share/X11
cp -Lrf /usr/share/terminfo ${ROOTFS}/usr/share
cp -Lrf /usr/share/vim ${ROOTFS}/usr/share
cp /lib64/ld-linux-x86-64.so.2 ${ROOTFS}/lib64
cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    ${ROOTFS}/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   ${ROOTFS}/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so ${ROOTFS}/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 ${ROOTFS}/usr/lib/x86_64-linux-gnu

cp /usr/lib/x86_64-linux-gnu/libyang.so.3  ${ROOTFS}/usr/lib/x86_64-linux-gnu

cp -v /usr/lib/file/magic.mgc ${ROOTFS}/usr/share/misc
cp -f /etc/bash_completion ${ROOTFS}/etc
cp -Lrf /usr/lib/locale ${ROOTFS}/usr/lib
cp -f /etc/locale.conf ${ROOTFS}/etc
cp -f /etc/locale.gen ${ROOTFS}/etc
cp -f /etc/default/locale ${ROOTFS}/etc/default
cp -f /usr/share/bash-completion/bash_completion ${ROOTFS}/usr/share/bash-completion
for i in "arp" "arping" "chmod" "chown" "curl" "file" \
         "find" "hostname" "id" "ip" "iperf3" "iptables" \
         "kill" "killall" "locale-gen" "lsof" "passwd" \
         "pgrep" "ping" "pwd" "route" "sh" "ssh" "strace" \
         "strings" "tcpdump" "tracepath" "wget" "watch" \
         "pkill" ; do
  cp -f /usr/share/bash-completion/completions/${i} ${ROOTFS}/usr/share/bash-completion/completions 
done
#----------------------------------------------------------------------------#
cd ${ROOTFS}/usr/bin
ln -s vim.basic vim
ln -s vim.basic vi
ln -s mawk awk
cd ${ROOTFS}/bin
ln -s bash sh
cd ${ROOTFS}/usr/sbin
ln -s iptables-nft iptables
cd ${HERE}
cat > ${ROOTFS}/etc/hosts << "EOF"
127.0.0.1	localhost
EOF
cat > ${ROOTFS}/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 ${ROOTFS}/etc/passwd
cat > ${ROOTFS}/etc/group << "EOF"
root:x:0:
tty:x:5:
EOF
chmod 644 ${ROOTFS}/etc/group
cat > ${ROOTFS}/etc/rsyslog.conf << "EOF"
module(load="imuxsock") # provides support for local system logging
$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
auth,authpriv.*                 /var/log/auth.log
cron.*                          -/var/log/cron.log
EOF
#----------------------------------------------------------------------------#
cp -f /usr/lib/x86_64-linux-gnu/libnss_files.so.2 ${ROOTFS}/lib/x86_64-linux-gnu
for i in "/etc/pam.conf" "/etc/login.defs" "/etc/shadow" ; do
  cp -f ${i} ${ROOTFS}/etc
done
#----------------------------------------------------------------------------#
cp -Lrf /usr/lib/frr ${ROOTFS}/usr/lib
cp -Lrf /usr/lib/x86_64-linux-gnu/frr ${ROOTFS}/usr/lib/x86_64-linux-gnu/
cp -r /lib/x86_64-linux-gnu/security ${ROOTFS}/lib/x86_64-linux-gnu
cp -f /lib/x86_64-linux-gnu/security/pam_deny.so ${ROOTFS}/usr/lib/security
cp -f /lib/x86_64-linux-gnu/security/pam_unix.so ${ROOTFS}/usr/lib/security
cp -f /lib/x86_64-linux-gnu/security/pam_permit.so ${ROOTFS}/usr/lib/security
cp -f /usr/lib/x86_64-linux-gnu/libnss_dns.so.2 ${ROOTFS}/usr/lib/x86_64-linux-gnu/
cp -f /usr/lib/x86_64-linux-gnu/libcap.so.2 ${ROOTFS}/usr/lib/x86_64-linux-gnu/

cp /etc/debian_version  ${ROOTFS}/etc

for i in "/etc/security" \
         "/etc/pam.d" \
         "/etc/frr" ; do
  cp -Lrf ${i} ${ROOTFS}/etc
done
#----------------------------------------------------------------------------#
for i in "usr/bin" "usr/sbin" "bin" "sbin" \
         "lib/x86_64-linux-gnu/security" \
         "usr/lib/x86_64-linux-gnu/frr" ; do
  /root/ldd_list_cp ${ROOTFS}/${i} ${ROOTFS}
done
#----------------------------------------------------------------------------#
chroot ${ROOTFS} passwd root <<EOF
root
root
EOF
#----------------------------------------------------------------------------#
cat > ${ROOTFS}/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/usr/sbin/rsyslogd
EOF
chmod +x ${ROOTFS}/usr/bin/cloonix_startup_script.sh
#----------------------------------------------------------------------------#
cd ${ROOTFS}
#----------------------------------------------------------------------------#
zip -yrq /root/zipbasic.zip .
#----------------------------------------------------------------------------#


