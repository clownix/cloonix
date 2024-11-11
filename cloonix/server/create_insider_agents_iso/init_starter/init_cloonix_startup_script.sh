#!/bin/sh
export LD_LIBRARY_PATH="/mnt/cloonix_config_fs/lib"
/mnt/cloonix_config_fs/cloonix-agent
/mnt/cloonix_config_fs/cloonix-dropbear-sshd
unset LD_LIBRARY_PATH
if [ -e /mnt/cloonix_config_fs/startup_nb_eth ]; then
  NB_ETH=$(cat /mnt/cloonix_config_fs/startup_nb_eth)
  NB_ETH=$((NB_ETH-1))
  for i in $(seq 0 $NB_ETH); do
    LIST="${LIST} eth$i"
  done
  for eth in $LIST; do
    VAL=$(ip link show | grep ": ${eth}@")
    while [  $(ip link show | egrep -c ": ${eth}") = 0 ]; do
      sleep 1
    done
  done
fi
if [ -x /usr/bin/cloonix_startup_script.sh ]; then
  /usr/bin/cloonix_startup_script.sh
fi
while [ 1 ]; do
  sleep 54321
done
