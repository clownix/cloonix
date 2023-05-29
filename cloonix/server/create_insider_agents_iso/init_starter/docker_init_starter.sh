#!/bin/sh

/mnt/cloonix_config_fs/cloonix-agent
/mnt/cloonix_config_fs/cloonix-dropbear-sshd

env > /tmp/launch_startup_env

__CUSTOMER_LAUNCHER_SCRIPT__

