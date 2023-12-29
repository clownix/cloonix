#!/bin/bash
HERE=`pwd`
XORRISO=/usr/bin/xorrisofs
TARGET=/tmp/cisco_initial_configuration

#
# The following id_rsa.pub is associated to the private id_rsa that are in
# hide_dirs.c used by cloonix_osh and cloonix_ocp.
cat > /tmp/cloonix_public_id_rsa.pub << EOF
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDGR6miK5hkP8eelcvitoRqsmLsSwwWSvk2gL9y/1IXwI/B4gUkzrqp7QJR30nD3s6/26VMs+ehRBgVuNv9ps5rt62KNHHe+y9qGBBKNik4FcKeCR3OigC9bKYtiwGxlCjRA21d1qA/SWa2Ll4Rf7+4K//4hYYXHu0FPxQXyznwOu4NGmZ3DMaCH9+vYXA6SXGNA7vWCQjN39+lBd6+yP3cKxQfdTNOu9OfwGt+soAEAB4fZB3XLFMdE9x/2zITMwwDxi3AI887y+qhsy3W0fpiuX9zsOTGoJ+1aM9XHSYwn9IcE5A/g0xWv58TpHRjVa5ZXCiOzzq4sC04UGXmL5Wb cisco@debian
EOF
ID_RSA=/tmp/cloonix_public_id_rsa.pub
#

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
mkdir -p ${TARGET}/cdrom
cp ${HERE}/configs/iosxe_config.txt ${TARGET}/cdrom
split -b 70 ${ID_RSA}
for i in xaa xab xac xad xae xaf; do
  var="$(cat ${i})"
  export "$(eval echo \$i=$var)"
  sed -i s%__${i}__%"$(eval echo \$$i)"% ${TARGET}/cdrom/iosxe_config.txt
  rm -f ${i}
done
${XORRISO} -o ${TARGET}/iosxe_config.iso ${TARGET}/cdrom
