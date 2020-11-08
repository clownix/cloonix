#!/bin/bash
HERE=`pwd`
CLOONIX_QEMU_BIN="/usr/local/bin/cloonix/server/qemu"

if [ ! -e ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 ]; then
  echo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 does not exist
  echo Install cloonix
  exit 1
fi
if [ ! -d ${HOME}/cloonix_data/bulk ]; then
  echo directory bulk does not exist:
  echo mkdir -p ${HOME}/cloonix_data/bulk
  exit 1
fi

VM=$1

QCOW2=${HOME}/cloonix_data/bulk/${VM}

if [ ! -f $QCOW2 ]; then
  echo $QCOW2
  echo does not exist
  exit
fi




echo
sudo ip tuntap add dev tap71 mode tap

sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
            -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
            -cpu host,+vmx -smp 4 \
            -serial stdio \
            -drive file=${QCOW2},index=0,media=disk,if=virtio \
            -netdev type=tap,id=net71,ifname=tap71 \
            -device virtio-net-pci,netdev=net71
echo
sudo ip link set tap71 down 
sudo ip tuntap del dev tap71 mode tap
echo


# echo
# sudo ip tuntap add dev tap71 mode tap
# sudo ip tuntap add dev tap72 mode tap
# sudo ip tuntap add dev tap73 mode tap
# sudo ip tuntap add dev tap74 mode tap
# sudo ip tuntap add dev tap75 mode tap
# 
# sudo ${CLOONIX_QEMU_BIN}/qemu-system-x86_64 \
#             -L ${CLOONIX_QEMU_BIN} -enable-kvm -m 6000 \
#             -cpu host,+vmx -smp 4 -no-reboot \
#             -serial stdio \
#             -drive file=${QCOW2},index=0,media=disk,if=virtio \
#             -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1 \
#             -netdev type=tap,id=net71,ifname=tap71 \
#             -device virtio-net-pci,netdev=net71,mac=52:54:00:81:f5:40 \
#             -netdev type=tap,id=net72,ifname=tap72 \
#             -device virtio-net-pci,netdev=net72,mac=52:54:00:81:f5:42 \
#             -netdev type=tap,id=net73,ifname=tap73 \
#             -device virtio-net-pci,netdev=net73,mac=52:54:00:81:f5:43 \
#             -netdev type=tap,id=net74,ifname=tap74 \
#             -device virtio-net-pci,netdev=net74,mac=52:54:00:81:f5:44 \
#             -netdev type=tap,id=net75,ifname=tap75 \
#             -device virtio-net-pci,netdev=net75,mac=52:54:00:81:f5:45 
# echo
# sudo ip tuntap del dev tap71 mode tap
# sudo ip tuntap del dev tap72 mode tap
# sudo ip tuntap del dev tap73 mode tap
# sudo ip tuntap del dev tap74 mode tap
# sudo ip tuntap del dev tap75 mode tap
# echo
# 
# 
# 
# 
