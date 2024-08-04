#!/bin/bash

printf "\n\n\tTO LAUNCH THE DEMO:\n\n"
printf "cd self_extracting_rootfs_dir\n"
printf "./crun_container_startup.sh\n\n"
printf "\tYou are now inside a CRUN container\n\n"
printf "./ping_demo.sh\n\n"
printf "\tTo get kvm to work, the devices:\n"
printf "\tUse setfacl or chmod to have read/write by your user of devices:\n"
printf "\t/dev/kvm /dev/vhost-net /dev/net/tun\n\n\n"
printf "\tTo have more virtual machines or containers, the bulk directory\n"
printf "\tis mounted to /var/lib/cloonix/bulk in the container, and this \n"
printf "\tis the place from which cloonix takes its images.\n\n\n"

