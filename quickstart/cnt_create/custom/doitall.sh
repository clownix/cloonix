#!/bin/bash

HERE=`pwd`

cd ${HERE}/busybox
./create_busybox.sh

cd ${HERE}/cloonix
./cnt_cloonix.sh

cd ${HERE}/dns_dhcp
./create_dns_dhcp.sh

cd ${HERE}/frr
./doitall.sh

cd ${HERE}/ovswitch
./create_ovswitch


