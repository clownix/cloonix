#!/bin/sh

/usr/bin/cloonix_parrot_srv

__CUSTOMER_LAUNCHER_SCRIPT__

count=0
while [ 1 ]; do
  sleep 60
  count=$((count+1))
  echo $count 
  printf "${count}mn since start\n" > /tmp/cloonix_init_starter.count
done
