#!/bin/bash
HERE=`pwd`
DEST="/usr/libexec/cloonix/cloonfs/bin"

./cmd

sudo mv ${HERE}/cloonix-gui ${DEST}
sudo chown root:root ${DEST}/cloonix-gui
sudo chmod 755 ${DEST}/cloonix-gui

./allclean
