#!/bin/bash
set -e

LIST="openwrt  \
      lunar    \
      bookworm \
      fedora38 \
      centos9  "

sudo echo "sudoer rights given"
mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.img ]; then
    echo /var/lib/cloonix/bulk/${i}.img
    echo must be erased first
    exit 1
  fi
  sudo rm -f /root/${i}
  sudo rm -f /root/${i}.img
done

for i in ${LIST}; do
  echo BEGIN ${i} 
  sudo ./${i}
  sudo mv -v /root/${i}.img /var/lib/cloonix/bulk
  echo END ${i}  
done
