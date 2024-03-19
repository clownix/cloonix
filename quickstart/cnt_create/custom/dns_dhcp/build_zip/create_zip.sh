#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/delv \
      /usr/bin/dig \
      /usr/bin/dnstap-read \
      /usr/bin/mdig \
      /usr/bin/nslookup \
      /usr/bin/nsupdate \
      /usr/bin/dnssec-cds \
      /usr/bin/dnssec-dsfromkey \
      /usr/bin/dnssec-keyfromlabel \
      /usr/bin/dnssec-keygen \
      /usr/bin/dnssec-revoke \
      /usr/bin/dnssec-settime \
      /usr/bin/dnssec-signzone \
      /usr/bin/dnssec-verify \
      /usr/bin/named-checkconf \
      /usr/bin/named-checkzone \
      /usr/bin/named-compilezone \
      /usr/sbin/rndc \
      /usr/sbin/rndc-confgen \
      /usr/bin/arpaname \
      /usr/bin/dnssec-importkey \
      /usr/bin/named-journalprint \
      /usr/bin/named-nzd2nzf \
      /usr/bin/named-rrchecker \
      /usr/bin/nsec3hash \
      /usr/sbin/ddns-confgen \
      /usr/sbin/named \
      /usr/sbin/tsig-keygen \
      /usr/bin/omshell \
      /usr/sbin \
      /usr/sbin/dhcp-lease-list \
      /usr/sbin/dhcpd \
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

rm -rf /root/dns_dhcp

for i in "bin" "lib64" "root" "run" "tmp" "usr/bin" \
         "etc/bind" "etc/dhcp" "var/run" "var/log" \
         "usr/share/dns" "var/spool/rsyslog" \
         "var/cache/bind" "var/lib/dhcp" \
         "usr/lib/x86_64-linux-gnu/rsyslog" ; do
  mkdir -p /root/dns_dhcp/${i}
done

for i in ${LIST}; do
  cp ${i} /root/dns_dhcp/bin
done

cp /lib64/ld-linux-x86-64.so.2 /root/dns_dhcp/lib64

${HERE}/cplibdep /root/dns_dhcp/bin /root/dns_dhcp

cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/dns_dhcp/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/dns_dhcp/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/dns_dhcp/usr/lib/x86_64-linux-gnu/rsyslog

cp -rf /etc/bind/* /root/dns_dhcp/etc/bind
cp -rf /etc/dhcp/* /root/dns_dhcp/etc/dhcp
cp -rf /usr/share/dns/* /root/dns_dhcp/usr/share/dns

touch /root/dns_dhcp/var/lib/dhcp/dhcpd.leases

cat > /root/dns_dhcp/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/dns_dhcp/etc/passwd

cat > /root/dns_dhcp/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/dns_dhcp/etc/group

cat > /root/dns_dhcp/etc/rsyslog.conf << "EOF"
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


cd /root/dns_dhcp/bin
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
         "lzop" "lzopcat" "md5sum" "mdev" "microcom" "mkdir" "mkfifo" \
         "mknod" "mkswap" "mktemp" "modinfo" "modprobe" "more" "mount" "mt" \
         "mv" "nameif" "nc" "od" "openvt" "patch" "pidof" "pivot_root" \
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

cat > /root/dns_dhcp/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/bin/rsyslogd
EOF
chmod +x /root/dns_dhcp/usr/bin/cloonix_startup_script.sh


cd /root/dns_dhcp

zip -yr ../dns_dhcp.zip .


