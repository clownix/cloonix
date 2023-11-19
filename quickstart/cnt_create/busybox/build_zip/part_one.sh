#!/bin/bash

HERE=`pwd`

if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi

LIST="/usr/bin/monit \
      /usr/sbin/rsyslogd \
      /bin/busybox \
      /usr/bin/ldd \
      /usr/bin/tail \
      /usr/bin/nohup \
      /bin/bash \
      /bin/netstat \
      /bin/ping \
      /sbin/ip \
      /sbin/ifconfig"

for i in ${LIST}; do
  if [ ! -e "${i}" ]; then
    echo ERROR ${i} not found
    exit
  fi
done

rm -rf /root/busybox
mkdir -p /root/busybox/bin

for i in ${LIST}; do
  cp ${i} /root/busybox/bin
done

mkdir -p /root/busybox/lib64
cp /lib64/ld-linux-x86-64.so.2 /root/busybox/lib64

${HERE}/cplibdep /root/busybox/bin /root/busybox

mkdir -p /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/lmnet.so    /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imklog.so   /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog
cp /usr/lib/x86_64-linux-gnu/rsyslog/imuxsock.so /root/busybox/usr/lib/x86_64-linux-gnu/rsyslog

