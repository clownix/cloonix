#!/bin/bash
set -e

LIST="openwrt  \
      lunar    \
      jammy    \
      bookworm \
      bullseye \
      fedora38 \
      fedora37 \
      centos9  \
      centos8  \
      opensuse155"

sudo echo "sudoer rights given"
mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.qcow2 ]; then
    echo /var/lib/cloonix/bulk/${i}.qcow2
    echo must be erased first
    exit 1
  fi
  sudo rm -f /root/${i}
  sudo rm -f /root/${i}.qcow2
done

for i in ${LIST}; do
  echo BEGIN ${i} 
  sudo ./${i}
  sudo mv -v /root/${i}.qcow2 /var/lib/cloonix/bulk
  echo END ${i}  
done
./tumbleweed
