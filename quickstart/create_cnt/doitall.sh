#!/bin/bash

sudo ./bookworm
sudo ./bullseye
sudo ./centos9
sudo ./fedora36
sudo ./jammy
sudo ./opensuse155
sudo ./openwrt
if [ -e /usr/bin/podman ]; then
  sudo ./podman_bookworm.sh
fi
if [ -e /usr/bin/docker ]; then
  sudo ./docker_bookworm.sh
fi
