#!/bin/bash
set -e

LIST="fedora40 \
      noble    \
      bookworm"

mkdir -p /var/lib/cloonix/bulk

for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.zip ]; then
    echo /var/lib/cloonix/bulk/${i}.zip
    echo already exists
  else
    echo BEGIN ${i} 
    sudo ./${i}
    echo END ${i}  
  fi
done
