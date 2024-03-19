#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/iperf3 \
      /usr/sbin/rsyslogd \
      /bin/busybox \
      /usr/bin/ldd \
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

rm -rf /root/busybox
mkdir -p /root/busybox

for i in "bin" "lib64" "root" "etc" "run" "tmp" \
         "var/log" "usr/bin" \
         "usr/lib/x86_64-linux-gnu/rsyslog" \
         "var/spool/rsyslog" ; do
  mkdir -v -p /root/busybox/${i}
done

for i in ${LIST}; do
  cp -v ${i} /root/busybox/bin
done

cp /lib64/ld-linux-x86-64.so.2 /root/busybox/lib64

sync

${HERE}/cplibdep /root/busybox/bin /root/busybox

cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog


cat > /root/busybox/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/busybox/etc/passwd

cat > /root/busybox/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/busybox/etc/group

cat > /root/busybox/etc/rsyslog.conf << "EOF"
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


cd /root/busybox/bin
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
         "mkswap" "mktemp" "modinfo" "modprobe" "more" "mount" "mt" "mv" \
         "nameif" "nc" "nslookup" "od" "openvt" "patch" "pidof" "pivot_root" \
         "poweroff" "printf" "ps" "pwd" "rdate" "readlink" "realpath" \
         "reboot" "renice" "reset" "rev" "rm" "rmdir" "rmmod" "route" \
         "rpm" "rpm2cpio" "run-parts" "sed" "seq" "setkeycodes" "setsid" \
         "sh" "sha1sum" "sha256sum" "sha512sum" "sleep" "sort" \
         "start-stop-daemon" "stat" "strings" "stty" "swapoff" "swapon" \
         "switch_root" "sync" "sysctl" "syslogd" "tac" "tar" "taskset" \
         "tee" "telnet" "test" "tftp" "time" "timeout" "top" "touch" "tr" \
         "traceroute" "traceroute6" "true" "tty" "udhcpc" "udhcpd" "umount" \
         "uname" "uncompress" "unexpand" "uniq" "unix2dos" "unlzma" \
         "unlzop" "unxz" "unzip" "uptime" "usleep" "uudecode" "uuencode" \
         "vconfig" "vi" "watch" "watchdog" "wc" "wget" "which" "who" \
         "whoami" "xargs" "xz" "xzcat" "yes" "zcat"; do
  ln -s busybox $i
done

cat > /root/busybox/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/bin/rsyslogd
EOF
chmod +x /root/busybox/usr/bin/cloonix_startup_script.sh

cd /root/busybox

zip -yr ../busybox.zip .


