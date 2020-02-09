#!/bin/bash
HERE=`pwd`
XORRISO=/usr/bin/xorrisofs
TARGET=/tmp/cisco_initial_configuration
ID_RSA=${HERE}/../../cloonix/id_rsa.pub
if [ ! -d ${BULK} ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${BULK}
  exit 1
fi
if [ ! -e ${ID_RSA} ]; then
  echo Not found:
  echo ${ID_RSA}
  exit 1
fi
rm -rf ${TARGET}
rm -f ${HERE}/preconfig_cisco.iso
mkdir ${TARGET}
cp ${HERE}/iosxe_config.txt ${TARGET}
cd ${TARGET}
split -b 70 ${ID_RSA}
for i in xaa xab xac xad xae xaf; do
  var="$(cat ${i})"
  export "$(eval echo \$i=$var)"
  sed -i s%__${i}__%"$(eval echo \$$i)"% iosxe_config.txt
  rm -f ${i}
done

${XORRISO} -o ${HERE}/preconfig_cisco.iso ${TARGET}
rm -rf ${TARGET}
