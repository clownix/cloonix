#!/bin/bash
set -e

LIST="fedora39 \
      centos9  \
      jammy    \
      bookworm"

mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.tar ]; then
    echo /var/lib/cloonix/bulk/${i}.tar
    echo already exists
  else
    echo BEGIN ${i} 
    sudo ./${i}
    echo END ${i}  
  fi
done
