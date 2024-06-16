#!/bin/bash

HERE=`pwd`

cd ${HERE}/busybox
./create_busybox.sh

cd ${HERE}/cloonix
./podman_cloonix.sh

cd ${HERE}/frr
./doitall.sh


