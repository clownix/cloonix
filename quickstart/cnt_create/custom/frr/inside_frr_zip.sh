#!/bin/bash

HERE=`pwd`
ROOTNM="frr"

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/iperf3 \
      /usr/sbin/rsyslogd \
      /usr/bin/file \
      /usr/bin/strace \
      /bin/busybox \
      /usr/bin/ldd \
      /usr/bin/tail \
      /usr/bin/nohup \
      /bin/bash \
      /bin/netstat \
      /bin/ping \
      /sbin/ip \
      /sbin/ifconfig \
      /sbin/sysctl \
      /usr/sbin/nologin \
      /usr/bin/ssh \
      /usr/bin/scp"

for i in ${LIST}; do
  if [ ! -e "${i}" ]; then
    echo ERROR ${i} not found
    exit
  fi
done

rm -rf /root/${ROOTNM}
mkdir -p /root/${ROOTNM}

for i in "bin" "lib64" "lib" "root" "etc" "tmp" "var/log" "usr/bin" \
         "usr/lib/x86_64-linux-gnu/rsyslog" "var/spool/rsyslog" \
         "lib/security" "etc/pam.d" "var/lib" "usr/lib/security" ; do
  mkdir -v -p /root/${ROOTNM}/${i}
done


for i in ${LIST}; do
  cp -v ${i} /root/${ROOTNM}/bin
done

cp /lib64/ld-linux-x86-64.so.2 /root/${ROOTNM}/lib64

${HERE}/cplibdep /root/${ROOTNM}/bin /root/${ROOTNM}

cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/${ROOTNM}/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/${ROOTNM}/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/${ROOTNM}/usr/lib/x86_64-linux-gnu/rsyslog

#FRR:
cp -f /usr/bin/mtracebis /root/${ROOTNM}/usr/bin 
cp -f /usr/bin/vtysh /root/${ROOTNM}/usr/bin
cp -Lrf /etc/frr /root/${ROOTNM}/etc
cp -Lrf /usr/lib/frr /root/${ROOTNM}/usr/lib
cp -Lrf /usr/lib/x86_64-linux-gnu/frr /root/${ROOTNM}/usr/lib/x86_64-linux-gnu/

#PAM
#cp -f /usr/lib/x86_64-linux-gnu/libpam.so.0 /root/${ROOTNM}/usr/lib/x86_64-linux-gnu
cp -f /usr/lib/x86_64-linux-gnu/security/pam_permit.so /root/${ROOTNM}/usr/lib/security
#cp -Lrf /usr/lib/x86_64-linux-gnu/security /root/${ROOTNM}/usr/lib/x86_64-linux-gnu


${HERE}/cplibdep /root/${ROOTNM}/usr/bin /root/${ROOTNM}
${HERE}/cplibdep /root/${ROOTNM}/usr/lib/frr /root/${ROOTNM}

cat > /root/${ROOTNM}/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 644 /root/${ROOTNM}/etc/passwd

cat > /root/${ROOTNM}/etc/group << "EOF"
root:x:0:
EOF
chmod 644 /root/${ROOTNM}/etc/group

cat > /root/${ROOTNM}/etc/rsyslog.conf << "EOF"
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


cd /root/${ROOTNM}/bin
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

cat > /root/${ROOTNM}/etc/sysctl.conf << "EOF"
net.ipv4.ip_forward=1
net.ipv4.conf.all.log_martians=1
EOF

for i in "run/frr" "var/run/frr" "var/tmp/frr" ; do
  mkdir -v -p /root/${ROOTNM}/${i}
  chmod 777 /root/${ROOTNM}/${i}
done

cat >> /root/${ROOTNM}/etc/passwd << "EOF"
frr:x:100:109:Frr routing suite,,,:/nonexistent:/bin/nologin
EOF

cat >> /root/${ROOTNM}/etc/group << "EOF"
frrvty:x:108:frr
frr:x:109:
EOF

cat > /root/${ROOTNM}/etc/pam.d/frr << "EOF"
auth sufficient pam_permit.so
EOF

cat > /root/${ROOTNM}/etc/frr/daemons << "EOF"
ospfd=yes
vtysh_enable=yes
zebra_options="  -A 127.0.0.1 -s 90000000"
ospfd_options="  -A 127.0.0.1"
EOF

cat > /root/${ROOTNM}/etc/frr/frr.conf << "EOF"
frr defaults traditional
log syslog debugging
router ospf
ospf router-id __NODE_ID__.1.1.1
redistribute connected
redistribute static
EOF
cat > /root/${ROOTNM}/usr/bin/cloonix_startup_script.sh << "EOF"
#!/bin/bash
/bin/rsyslogd
/bin/sysctl -p

NB=1
case ${NODE_ID} in

  "tod1")
    NB=1
    IP0=10
    IP1=11
    ;;

  "tod2")
    NB=2
    IP0=11
    IP1=12
    ;;

  "tod3")
    NB=3
    IP0=12
    IP1=13
    ;;

  "tod4")
    NB=4
    IP0=13
    IP1=14
    ;;

  "tod5")
    NB=5
    IP0=14
    IP1=10
    ;;

  "cod1")
    NB=6
    IP0=10
    IP1=15
    IP2=16
    IP3=17
    ;;

  "cod2")
    NB=7
    IP0=11
    IP1=18
    IP2=19
    IP3=23
    ;;

  "cod3")
    NB=8
    IP0=12
    IP1=24
    IP2=20
    IP3=25
    ;;

  "cod4")
    NB=9
    IP0=13
    IP1=26
    IP2=21
    IP3=27
    ;;

  "cod5")
    NB=10
    IP0=14
    IP1=28
    IP2=22
    IP3=29
    ;;

  "nod1")
    NB=11
    IP0=17
    IP1=18
    ;;

  "nod2")
    NB=12
    IP0=23
    IP1=24
    ;;

  "nod3")
    NB=13
    IP0=25
    IP1=26
    ;;

  "nod4")
    NB=14
    IP0=27
    IP1=28
    ;;

  "nod5")
    NB=15
    IP0=15
    IP1=29
    ;;

  "nod11")
    NB=16
    IP0=15
    IP1=30
    ;;

  "nod12")
    NB=17
    IP0=16
    IP1=31
    ;;

  "nod13")
    NB=18
    IP0=17
    IP1=32
    ;;

  "nod21")
    NB=19
    IP0=18
    IP1=33
    ;;

  "nod22")
    NB=20
    IP0=19
    IP1=34
    ;;

  "nod23")
    NB=21
    IP0=23
    IP1=35
    ;;

  "nod31")
    NB=22
    IP0=24
    IP1=36
    ;;

  "nod32")
    NB=23
    IP0=20
    IP1=37
    ;;

  "nod33")
    NB=24
    IP0=25
    IP1=38
    ;;

  "nod41")
    NB=25
    IP0=26
    IP1=39
    ;;

  "nod42")
    NB=26
    IP0=21
    IP1=40
    ;;

  "nod43")
    NB=27
    IP0=27
    IP1=41
    ;;

  "nod51")
    NB=28
    IP0=28
    IP1=42
    ;;

  "nod52")
    NB=29
    IP0=22
    IP1=43
    ;;

  "nod53")
    NB=30
    IP0=29
    IP1=44
    ;;

  "lod11")
    NB=31
    IP0=30
    IP1=31
    ;;

  "lod12")
    NB=32
    IP0=31
    IP1=32
    ;;

  "lod13")
    NB=33
    IP0=32
    IP1=33
    ;;

  "lod21")
    NB=34
    IP0=33
    IP1=34
    ;;

  "lod22")
    NB=35
    IP0=34
    IP1=35
    ;;

  "lod23")
    NB=36
    IP0=35
    IP1=36
    ;;

  "lod31")
    NB=37
    IP0=36
    IP1=37
    ;;

  "lod32")
    NB=38
    IP0=37
    IP1=38
    ;;

  "lod33")
    NB=39
    IP0=38
    IP1=39
    ;;

  "lod41")
    NB=40
    IP0=39
    IP1=40
    ;;

  "lod42")
    NB=41
    IP0=40
    IP1=41
    ;;

  "lod43")
    NB=42
    IP0=41
    IP1=42
    ;;

  "lod51")
    NB=43
    IP0=42
    IP1=43
    ;;

  "lod52")
    NB=44
    IP0=43
    IP1=44
    ;;

  "lod53")
    NB=45
    IP0=44
    IP1=30
    ;;

  "sod30"|"sod31"|"sod32"|"sod33"|"sod34"|"sod35"|"sod36"|"sod37"|"sod38"|"sod39")
    IP0=${NODE_ID#sod}
    ;;
  "sod40"|"sod41"|"sod42"|"sod43"|"sod44")
    IP0=${NODE_ID#sod}
    ;;

   *)
    /bin/logger "STARTUP FAILURE NODE_ID=$NODE_ID"


esac

sed -i s"/__NODE_ID__/${NB}/" /etc/frr/frr.conf

if [ ! -z "$IP0" ]; then
  ifconfig eth0 ${IP0}.0.0.${NB}/24 up
  echo "network ${IP0}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP1" ]; then
  ifconfig eth1 ${IP1}.0.0.${NB}/24 up
  echo "network ${IP1}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP2" ]; then
  ifconfig eth2 ${IP2}.0.0.${NB}/24 up
  echo "network ${IP2}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP3" ]; then
  ifconfig eth3 ${IP3}.0.0.${NB}/24 up
  echo "network ${IP3}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

/usr/lib/frr/frrinit.sh start
EOF
chmod +x /root/${ROOTNM}/usr/bin/cloonix_startup_script.sh

cat > /root/${ROOTNM}/etc/hosts << "EOF"
127.0.0.1  localhost
30.0.0.1   sod30
31.0.0.1   sod31
32.0.0.1   sod32
33.0.0.1   sod33
34.0.0.1   sod34
35.0.0.1   sod35
36.0.0.1   sod36
37.0.0.1   sod37
38.0.0.1   sod38
39.0.0.1   sod39
40.0.0.1   sod40
41.0.0.1   sod41
42.0.0.1   sod42
43.0.0.1   sod43
44.0.0.1   sod44
EOF


cd /root/${ROOTNM}

zip -yr ../${ROOTNM}.zip .


