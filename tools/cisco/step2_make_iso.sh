#!/bin/bash
HERE=`pwd`
XORRISO=/usr/bin/xorrisofs
BULK=${HOME}/cloonix_data/bulk
TARGET=/tmp/cisco_initial_configuration
if [ ! -d ${BULK} ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${BULK}
  exit 1
fi
rm -rf ${TARGET}
rm -f ${BULK}/preconfig_cisco.iso
mkdir ${TARGET}
cp ${HERE}/configs/iosxe_config.txt ${TARGET}
${XORRISO} -o ${BULK}/preconfig_cisco.iso ${TARGET}
rm -rf ${TARGET}
