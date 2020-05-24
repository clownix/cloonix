#!/bin/bash
HERE=`pwd`
TARGZSTORE=${HERE}/../../../targz_store
NAME=tldk
NAMEZ=${NAME}.tar.gz
tar xvf ${TARGZSTORE}/${NAMEZ}
mv ${NAME} ${NAME}_tainted
cd ${NAME}_tainted
patch -p1 < ../tldk.patch


