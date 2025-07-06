#!/bin/bash
HERE=`pwd`

cloonix_cli nemo kil 1>/dev/null 2>&1

cd ${HERE}
sudo rm -rf /var/lib/cloonix/bulk/alpine0
sudo rm -rf /var/lib/cloonix/bulk/alpine
sudo rm -rf /var/lib/cloonix/bulk/bookworm0
sudo rm -rf /var/lib/cloonix/bulk/bookworm

cd ${HERE}/alpine
./alpine0.sh
./alpine.sh

cd ${HERE}/bookworm
./bookworm0.sh
./bookworm.sh
