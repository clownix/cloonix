#!/bin/bash
set +e
/mnt/cloonix_config_fs/cloonix-agent
/mnt/cloonix_config_fs/cloonix-dropbear-sshd
echo "USER SCRIPT" > /tmp/init_cloonix_startup_script.log
env >> /tmp/init_cloonix_startup_script.log
if [ -e /mnt/cloonix_config_fs/startup_nb_eth ]; then
  echo "STARTUP_LOGGER ETH WAIT BEGIN" >> /tmp/init_cloonix_startup_script.log
  NB_ETH=$(cat /mnt/cloonix_config_fs/startup_nb_eth)
  NB_ETH=$((NB_ETH-1))
  for i in $(seq 0 $NB_ETH); do LIST="${LIST} eth$i";done
  for eth in $LIST; do
    echo "STARTUP_LOGGER TEST FOR $eth" >> /tmp/init_cloonix_startup_script.log
    VAL=$(ip link show | grep ": ${eth}@")
    echo $VAL >> /tmp/init_cloonix_startup_script.log
    while [  $(ip link show | egrep -c ": ${eth}") = 0 ]; do
      echo "STARTUP_LOGGER WAIT FOR $eth" >> /tmp/init_cloonix_startup_script.log
      sleep 1
    done
  done
  echo "STARTUP_LOGGER ETH WAIT END $NB_ETH" >> /tmp/init_cloonix_startup_script.log
fi
if [ -x /usr/bin/cloonix_startup_script.sh ]; then
  echo "START LAUNCH /usr/bin/cloonix_startup_script.sh" >> /tmp/init_cloonix_startup_script.log
  echo "while [ 1 ]; do sleep 123; done" >> /usr/bin/cloonix_startup_script.sh
  nohup /usr/bin/cloonix_startup_script.sh 1>/dev/null 2>&1 &
  echo "END LAUNCH /usr/bin/cloonix_startup_script.sh" >> /tmp/init_cloonix_startup_script.log
fi

while [ 1 ]; do
  sleep 3600
done

