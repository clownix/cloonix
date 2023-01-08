#!/bin/sh

/mnt/cloonix_config_fs/cloonix_agent
/mnt/cloonix_config_fs/dropbear_cloonix_sshd

env > /tmp/launch_startup_env

__CUSTOMER_LAUNCHER_SCRIPT__

