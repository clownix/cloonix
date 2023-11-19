#!/bin/sh
set +e
/mnt/cloonix_config_fs/cloonix-agent
/mnt/cloonix_config_fs/cloonix-dropbear-sshd
/mnt/cloonix_config_fs/podman_init_user.sh
while [ 1 ]; do
  sleep 3600
done

