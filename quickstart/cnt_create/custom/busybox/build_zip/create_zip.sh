#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

if [ ${#} = 1 ]; then
  case "$1" in
    "i386")
      ARCH="i386"
      NAME=busybox_i386
    ;;
    "amd64")
      ARCH="amd64"
      NAME=busybox
    ;;
    *)
    printf "\n\tERROR: $0 param must be i386, or amd64"
    exit
    ;;
  esac
else
  echo ERROR $0 param must be i386, or amd64
  exit
fi




LIST="/usr/bin/iperf3 \
      /usr/sbin/rsyslogd \
      /usr/bin/file \
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

rm -rf /root/${NAME}
mkdir -p /root/${NAME}

for i in "bin" "root" "etc" "run" "tmp" "lib" \
         "var/log" "usr/bin" "var/spool/rsyslog" ; do
  mkdir -v -p /root/${NAME}/${i}
done

for i in ${LIST}; do
  cp -v ${i} /root/${NAME}/bin
done

if [ ${ARCH} = "i386" ]; then
  mkdir -v -p /root/${NAME}/usr/lib/i386-linux-gnu/rsyslog
  cp /lib/ld-linux.so.2 /root/${NAME}/lib
  cp /usr/lib/i386-linux-gnu/rsyslog/lmnet.so    /root/${NAME}/usr/lib/i386-linux-gnu/rsyslog
  cp /usr/lib/i386-linux-gnu/rsyslog/imklog.so   /root/${NAME}/usr/lib/i386-linux-gnu/rsyslog
  cp /usr/lib/i386-linux-gnu/rsyslog/imuxsock.so /root/${NAME}/usr/lib/i386-linux-gnu/rsyslog
else
  mkdir -v -p /root/${NAME}/lib64
  mkdir -v -p /root/${NAME}/usr/lib/x86_64-linux-gnu/rsyslog
  cp /lib64/ld-linux-x86-64.so.2 /root/${NAME}/lib64
  cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/${NAME}/usr/lib/x86_64-linux-gnu/rsyslog
  cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/${NAME}/usr/lib/x86_64-linux-gnu/rsyslog
  cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/${NAME}/usr/lib/x86_64-linux-gnu/rsyslog
fi

mkdir -v -p /root/${NAME}/usr/share/misc
cp -v /usr/lib/file/magic.mgc /root/${NAME}/usr/share/misc
sync

${HERE}/cplibdep /root/${NAME}/bin /root/${NAME}



cat > /root/${NAME}/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/${NAME}/etc/passwd

cat > /root/${NAME}/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/${NAME}/etc/group

cat > /root/${NAME}/etc/rsyslog.conf << "EOF"
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


cd /root/${NAME}/bin
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

cat > /root/${NAME}/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/bin/rsyslogd
EOF
chmod +x /root/${NAME}/usr/bin/cloonix_startup_script.sh

cd /root/${NAME}

zip -yr ../${NAME}.zip .


