#!/bin/bash

HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"
BULK="${HOME}/cloonix_data/bulk"
NAME="mikrotik"
ISOSRC="https://download.mikrotik.com/routeros/7.6/mikrotik-7.6.iso"
ISO="${BULK}/mikrotik-7.6.iso"
QCOW2="${BULK}/${NAME}.qcow2"

if [ ! -e ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 ]; then
  echo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 does not exist
  echo Install cloonix
  exit 1
fi

if [ ! -d ${BULK} ]; then
  echo directory ${BULK} does not exist:
  echo mkdir -p ${BULK}
  exit 1
fi

cd ${BULK}
if [ ! -e ${ISO} ]; then
  wget ${ISOSRC}
  if [ ! -e ${ISO} ]; then
    echo ERROR could not get ${ISOSRC}
    exit 1
  fi
fi



echo 
echo 1 Press i to install.
echo 2 Press y to install.
echo 3 Press return on the ENTER to reboot message.
echo 4 The file mikrotik.qcow2 should have been created in bulk.
echo 
echo 5 Launch cloonix_net nemo.
echo 6 Launch cloonix_gui nemo.
echo 7 In cloonix_gui, right-click and pick kvm_conf.
echo 8 click on no_qemu_ga, natplug_flag, and persistent_rootfs.
echo 9 click on Choice and pick mikrotik.qcow2.
echo 10 click on n for the Eth0 to have no ethernet.
echo 11 click on OK.
echo 12 In cloonix_gui, right-click and pick kvm.
echo 13 The vm based on mikrotik.qcow2 should be visible WITH NO ETH.
echo 
echo 13 Right-click while above the vm, then pick the Console rather than the Spice.
echo 14 In the console press return, note that the Spice cannot handle copy-paste Console can.
echo 15 In the Console, put \"admin\" and return \(empty password\), choose a password....
echo 16 put n for the software licence, press enter and choose a password....
echo 17 When logged in as admin request dhclient at next startup by copy-paste in Console as follows:
printf "18 /system script add name=dhcpcnf source=\":delay 5;ip/dhcp-client/add interface=ether1\"\n"
echo 19 /system scheduler add name=dhcpcnf on-event=dhcpcnf start-time=startup interval=5
echo 20 Poweroff the vm with \"system/shutdown\".
echo 
echo 21 In cloonix_gui, right-click and pick kvm_conf.
echo 22 Unselect persistent_rootfs but keep no_qemu_ga and natplug_flag.
echo 23 click on s for the Eth0 Eth1 and Eth2 or whatever you whish as long as more than 1 eth.
echo 24 In cloonix_gui, right-click and pick kvm.
echo 25 After 10 seconds, a double-click on the vm should get a new terminal requesting the password.
echo 



rm -f ${BULK}/${NAME}.qcow2
qemu-img create -f qcow2 ${QCOW2} 5G
sync
sleep 1
sync

${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
     -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 2000 \
     -cpu host,+vmx \
     -smp 4 \
     -vga virtio \
     -nodefaults \
     -no-reboot \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -drive if=none,id=hd,file=${QCOW2} \
     -boot d \
     -cdrom ${ISO}

echo
echo
echo DONE ${QCOW2}

exit
    

${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
     -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 2000 \
     -cpu host,+vmx \
     -smp 4 \
     -vga virtio \
     -no-reboot \
     -nodefaults \
     -device virtio-scsi-pci,id=scsi \
     -device scsi-hd,drive=hd \
     -drive if=none,id=hd,file=${QCOW2}





