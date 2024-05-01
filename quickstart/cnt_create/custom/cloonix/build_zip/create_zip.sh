#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/unshare \
      /usr/bin/nsenter \
      /usr/bin/findmnt \
      /usr/bin/mount \
      /usr/bin/umount \
      /usr/bin/strace \
      /usr/bin/xeyes \
      /usr/sbin/rsyslogd \
      /bin/busybox \
      /usr/bin/ldd \
      /usr/bin/tar \
      /usr/bin/tail \
      /usr/bin/nohup \
      /bin/bash \
      /bin/netstat \
      /bin/ping \
      /sbin/ip \
      /sbin/ifconfig \
      /usr/bin/ssh \
      /usr/bin/scp"

for i in ${LIST}; do
  if [ ! -e "${i}" ]; then
    echo ERROR ${i} not found
    exit
  fi
done

rm -rf /root/podman_cloonix
for i in "bin" "lib64" "root" "etc" "run" "tmp" \
         "/dev/net" "usr/bin" "var/log" \
         "var/spool/rsyslog" "var/lib/cloonix/bulk" \
         "usr/libexec" \
         "usr/lib/x86_64-linux-gnu/rsyslog"; do
  mkdir -p /root/podman_cloonix/${i}
done

for i in ${LIST}; do
  cp ${i} /root/podman_cloonix/bin
done

cp /lib64/ld-linux-x86-64.so.2 /root/podman_cloonix/lib64

${HERE}/cplibdep /root/podman_cloonix/bin /root/podman_cloonix

cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/podman_cloonix/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/podman_cloonix/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/podman_cloonix/usr/lib/x86_64-linux-gnu/rsyslog


cat > /root/podman_cloonix/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/podman_cloonix/etc/passwd

cat > /root/podman_cloonix/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/podman_cloonix/etc/group

cat > /root/podman_cloonix/etc/rsyslog.conf << "EOF"
module(load="imuxsock") # provides support for local system logging
module(load="imklog")   # provides kernel logging support
$ActionFileDefaultTemplate RSYSLOG_TraditionalFileFormat
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
$IncludeConfig /etc/rsyslog.d/*.conf
*.*;auth,authpriv.none          -/var/log/syslog
auth,authpriv.*                 /var/log/auth.log
cron.*                          -/var/log/cron.log
kern.*                          -/var/log/kern.log
mail.*                          -/var/log/mail.log
user.*                          -/var/log/user.log
*.emerg                         :omusrmsg:*
EOF


cd /root/podman_cloonix/bin
for i in "[" "[[" "acpid" "adjtimex" "ar" "arp" "arping" "ash" "awk" \
         "basename" "blockdev" "brctl" "bunzip2" "bzcat" "bzip2" "cal" \
         "cat" "chgrp" "chmod" "chown" "chroot" "chvt" "clear" "cmp" \
         "cp" "cpio" "cttyhack" "cut" "date" "dc" "dd" "deallocvt" \
         "depmod" "devmem" "df" "diff" "dirname" "dmesg" "dnsdomainname" \
         "dos2unix" "du" "dumpkmap" "dumpleases" "echo" "egrep" "env" \
         "expand" "expr" "false" "fgrep" "find" "fold" "free" "freeramdisk" \
         "fstrim" "ftpget" "ftpput" "getopt" "getty" "grep" "groups" \
         "gunzip" "gzip" "halt" "head" "hexdump" "hostid" "hostname" \
         "httpd" "hwclock" "id" "init" "insmod" "ionice" "ipcalc" "kill" \
         "killall" "klogd" "last" "less" "ln" "loadfont" "loadkmap" "logger" \
         "login"  "logname" "logread" "losetup" "ls" "lsmod" "lzcat" "lzma" \
         "lzop" "lzopcat" "md5sum" "mdev" "microcom" "mkdir" "mkfifo" "mknod" \
         "mkswap" "mktemp" "modinfo" "modprobe" "more" "mt" "mv" \
         "nameif" "nc" "nslookup" "od" "openvt" "patch" "pidof" "pivot_root" \
         "poweroff" "printf" "ps" "pwd" "rdate" "readlink" "realpath" \
         "reboot" "renice" "reset" "rev" "rm" "rmdir" "rmmod" "route" \
         "rpm" "rpm2cpio" "run-parts" "sed" "seq" "setkeycodes" "setsid" \
         "sh" "sha1sum" "sha256sum" "sha512sum" "sleep" "sort" \
         "start-stop-daemon" "stat" "strings" "stty" "swapoff" "swapon" \
         "switch_root" "sync" "sysctl" "syslogd" "tac" "taskset" \
         "tee" "telnet" "test" "tftp" "time" "timeout" "top" "touch" "tr" \
         "traceroute" "traceroute6" "true" "tty" "udhcpc" "udhcpd" \
         "uname" "uncompress" "unexpand" "uniq" "unix2dos" "unlzma" \
         "unlzop" "unxz" "unzip" "uptime" "usleep" "uudecode" "uuencode" \
         "vconfig" "vi" "watch" "watchdog" "wc" "wget" "which" "who" \
         "whoami" "xargs" "xz" "xzcat" "yes" "zcat"; do
  ln -s busybox $i
done

cd /root
BUNDLE=$(ls cloonix-bundle*)
tar xvf ${BUNDLE}
rm -vf ${BUNDLE} 
BUNDLE=${BUNDLE%%.*}
mv /root/${BUNDLE} /root/podman_cloonix
chroot /root/podman_cloonix /bin/sh -c "cd ${BUNDLE}; ./install_cloonix"
rm -rf /root/podman_cloonix/${BUNDLE} 

cat > /root/podman_cloonix/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/bin/rsyslogd
mknod /dev/kvm c 10 232
mknod /dev/vhost-net c 10 238
mkdir /dev/net
mknod /dev/net/tun c 10 200
EOF
chmod +x /root/podman_cloonix/usr/bin/cloonix_startup_script.sh

cd /root/podman_cloonix
zip -yr ../podman_cloonix.zip .


