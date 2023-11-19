#!/bin/bash

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

if [ ! -d /root/busybox/bin ]; then
  echo ERROR /root/busybox/bin not found
  exit
fi

mkdir -p /root/busybox/root
mkdir -p /root/busybox/etc
mkdir -p /root/busybox/run
mkdir -p /root/busybox/tmp
mkdir -p /root/busybox/etc/monit/conf-enabled
mkdir -p /root/busybox/var/log
mkdir -p /root/busybox/var/spool/rsyslog

cat > /root/busybox/etc/passwd << "EOF"
root:x:0:0:root:/root:/bin/bash
EOF
chmod 600 /root/busybox/etc/passwd

cat > /root/busybox/etc/group << "EOF"
root:x:0:
EOF
chmod 600 /root/busybox/etc/group

cat > /root/busybox/etc/monit/monitrc << "EOF"
set daemon 120
set logfile /var/log/monit.log
include /etc/monit/conf-enabled/*
EOF
chmod 600 /root/busybox/etc/monit/monitrc

cat > /root/busybox/etc/monit/conf-enabled/rsyslog << "EOF"
check process rsyslogd with pidfile /var/run/rsyslogd.pid
    start program = "/bin/rsyslogd"             
EOF

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
for i in [ [[ acpid adjtimex ar arp arping ash awk basename blockdev \
         brctl bunzip2 bzcat bzip2 cal cat chgrp chmod chown chroot \
         chvt clear cmp cp cpio cttyhack cut date dc dd deallocvt \
         depmod devmem df diff dirname dmesg dnsdomainname dos2unix du \
         dumpkmap dumpleases echo egrep env expand expr false fgrep \
         find fold free freeramdisk fstrim ftpget ftpput getopt getty \
         grep groups gunzip gzip halt head hexdump hostid hostname \
         httpd hwclock id init insmod ionice ipcalc kill \
         killall klogd last less ln loadfont loadkmap logger login \
         logname logread losetup ls lsmod lzcat lzma lzop lzopcat \
         md5sum mdev microcom mkdir mkfifo mknod mkswap mktemp modinfo \
         modprobe more mount mt mv nameif nc nslookup od \
         openvt patch pidof pivot_root poweroff printf ps \
         pwd rdate readlink realpath reboot renice reset rev rm rmdir \
         rmmod route rpm rpm2cpio run-parts sed seq setkeycodes setsid \
         sh sha1sum sha256sum sha512sum sleep sort start-stop-daemon \
         stat strings stty swapoff swapon switch_root sync sysctl \
         syslogd tac tar taskset tee telnet test tftp time \
         timeout top touch tr traceroute traceroute6 true tty udhcpc \
         udhcpd umount uname uncompress unexpand uniq unix2dos unlzma \
         unlzop unxz unzip uptime usleep uudecode uuencode vconfig vi \
         watch watchdog wc wget which who whoami xargs xz xzcat yes \
         zcat; do
  ln -s busybox $i
done

cd /root/busybox

zip -yr ../busybox.zip .


