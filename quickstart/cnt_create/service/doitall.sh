#!/bin/bash
HERE=`pwd`
LIST="busybox \
      dns_dhcp \
      ovswitch \
      freeswitch"
mkdir -p /var/lib/cloonix/bulk
for i in ${LIST}; do
  if [ -e /var/lib/cloonix/bulk/${i}.zip ]; then
    echo /var/lib/cloonix/bulk/${i}.zip
    echo already exists
  else
    echo BEGIN ${i} 
    cd ${HERE}/${i}
    ./create_${i}.sh
    echo END ${i}  
  fi
done
