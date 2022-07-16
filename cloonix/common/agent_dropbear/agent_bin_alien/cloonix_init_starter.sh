#!/bin/sh

/mnt/cloonix_config_fs/cloonix_agent
/mnt/cloonix_config_fs/dropbear_cloonix_sshd

__CUSTOMER_LAUNCHER_SCRIPT__

while [ 1 ]; do
  sleep 3000
done

