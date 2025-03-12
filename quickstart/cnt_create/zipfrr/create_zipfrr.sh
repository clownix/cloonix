#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
ROOTFS="/tmp/zipfrr_rootfs"
BULK="/var/lib/cloonix/bulk"
BASIC="zipbasic.zip"
FRR="zipfrr.zip"
#----------------------------------------------------------------------------#
if [ ! -e ${BULK}/${BASIC} ]; then
  echo MISSING ${BULK}/${BASIC}
  exit 1
fi
rm -rf ${ROOTFS}
rm -f ${BULK}/${FRR}
mkdir -p ${ROOTFS}
cd ${ROOTFS}
echo QUIET UNZIP OF ${BULK}/${BASIC}
unshare -rm unzip -q ${BULK}/${BASIC} 
#----------------------------------------------------------------------------#

for i in "run/frr" "var/run/frr" "var/tmp/frr" ; do
  mkdir -v -p ${ROOTFS}/${i}
  chmod 777 ${ROOTFS}/${i}
done

cat >> ${ROOTFS}/etc/passwd << "EOF"
frr:x:0:0:Frr routing suite,,,:/nonexistent:/bin/nologin
EOF

cat >> ${ROOTFS}/etc/group << "EOF"
frrvty:x:0:frr
frr:x:0:
EOF

cat > ${ROOTFS}/etc/pam.d/frr << "EOF"
auth sufficient pam_permit.so
EOF

cat > ${ROOTFS}/etc/frr/daemons << "EOF"
ospfd=yes
vtysh_enable=yes
zebra_options="  -A 127.0.0.1 -s 90000000"
ospfd_options="  -A 127.0.0.1"
EOF

cat > ${ROOTFS}/etc/frr/frr.conf << "EOF"
frr defaults traditional
log syslog debugging
router ospf
ospf router-id __NODE_ID__.1.1.1
redistribute connected
redistribute static
EOF

cp -f ${HERE}/cloonix_startup_script.sh ${ROOTFS}/usr/bin
cp -f ${HERE}/hosts ${ROOTFS}/etc

#----------------------------------------------------------------------------#
cd ${ROOTFS}
echo QUIET ZIP OF ${BULK}/${FRR}
unshare -rm  zip -yrq ${BULK}/${FRR} .
echo DONE ${BULK}/${FRR}
#----------------------------------------------------------------------------#


