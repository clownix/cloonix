#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/ovs-dpctl \
      /usr/bin/ovs-dpctl-top \
      /usr/bin/ovs-pcap \
      /usr/bin/ovs-tcpdump \
      /usr/bin/ovs-tcpundump \
      /usr/bin/ovs-vlan-test \
      /usr/bin/ovs-vsctl \
      /usr/bin/ovs-appctl \
      /usr/bin/ovs-docker \
      /usr/bin/ovs-ofctl \
      /usr/bin/ovs-parse-backtrace \
      /usr/bin/ovs-pki \
      /usr/bin/ovsdb-client \
      /usr/bin/ovsdb-tool \
      /usr/sbin/ovs-bugtool \
      /usr/sbin/ovsdb-server \
      /usr/lib/openvswitch-switch/ovs-vswitchd \
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

rm -rf /root/ovswitch

for i in "bin" "lib64" "root" "run" "tmp" "var/run" "var/log" "usr/bin" \
         "usr/lib/x86_64-linux-gnu/rsyslog" "var/spool/rsyslog" \
         "etc/openvswitch" "/var/run/openvswitch" \
         "usr/share/openvswitch" "/var/log/openvswitch" \
         "usr/lib/openvswitch-switch"; do
  mkdir -p /root/ovswitch/${i}
done

for i in ${LIST}; do
  cp ${i} /root/ovswitch/bin
done

cp /lib64/ld-linux-x86-64.so.2 /root/ovswitch/lib64

${HERE}/cplibdep /root/ovswitch/bin /root/ovswitch

cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/ovswitch/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/ovswitch/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/ovswitch/usr/lib/x86_64-linux-gnu/rsyslog

cp -rf /usr/share/openvswitch/* /root/ovswitch/usr/share/openvswitch
cp -f /var/lib/openvswitch/conf.db /root/ovswitch/etc/openvswitch

cat > /root/ovswitch/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/ovswitch/etc/passwd

cat > /root/ovswitch/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/ovswitch/etc/group

cat > /root/ovswitch/etc/rsyslog.conf << "EOF"
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

cd /root/ovswitch/bin
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
  ln -s busybox ${i}
done

cat > /root/ovswitch/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash

/bin/rsyslogd

/bin/ovsdb-server /etc/openvswitch/conf.db -vconsole:emer -vsyslog:err -vfile:info --remote=punix:/var/run/openvswitch/db.sock --private-key=db:Open_vSwitch,SSL,private_key --certificate=db:Open_vSwitch,SSL,certificate --bootstrap-ca-cert=db:Open_vSwitch,SSL,ca_cert --no-chdir --log-file=/var/log/openvswitch/ovsdb-server.log --pidfile=/var/run/openvswitch/ovsdb-server.pid --detach
sleep 1
/bin/ovs-vswitchd unix:/var/run/openvswitch/db.sock -vconsole:emer -vsyslog:err -vfile:info --mlockall --no-chdir --log-file=/var/log/openvswitch/ovs-vswitchd.log --pidfile=/var/run/openvswitch/ovs-vswitchd.pid --detach

/bin/logger "STARTUP_BR0 BEGIN"
ovs-vsctl --no-wait add-br br0
for i in 0 1 2 3 4 5 6 7 8 9 10 11 12; do
  if [  $(ifconfig -a | egrep -c ^eth${i}) = 1 ] ; then
    ovs-vsctl add-port br0 eth${i}
  fi
done
/bin/logger "STARTUP_BR0 END"
EOF

chmod +x /root/ovswitch/usr/bin/cloonix_startup_script.sh

echo "nameserver 127.0.0.1" > /root/ovswitch/etc/resolv.conf
echo "127.0.0.1 localhost" > /root/ovswitch/etc/hosts

cd /root/ovswitch

zip -yr ../ovswitch.zip .


