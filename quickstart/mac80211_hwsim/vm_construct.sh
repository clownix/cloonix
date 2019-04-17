#######################################################################
vm_construct()
{ 
MIRROR=$1
DIST=$2
NET=$3
BULK=$4
VM_NAME=$5
PKT_LIST=${@:6}
echo
echo MIRROR=$MIRROR
echo DIST=$DIST
echo BULK=$BULK
echo VM_NAME=$VM_NAME
echo PKT_LIST=$PKT_LIST
echo

if [ ! -e ${BULK}/${DIST}.qcow2 ]; then
  echo ERROR:
  echo ${BULK}/${DIST}.qcow2
  echo not found!
  exit 1
fi

cp -v ${BULK}/${DIST}.qcow2 ${BULK}/${VM_NAME}.qcow2

cloonix_cli ${NET} add nat nat 
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add kvm ${VM_NAME} ram=2048 cpu=1 dpdk=0 sock=1 hwsim=0 ${VM_NAME}.qcow2 --persistent &

while ! cloonix_ssh ${NET} ${VM_NAME} "echo" 2>/dev/null; do
  echo ${VM_NAME} not ready, waiting 5 sec
  sleep 5
done

cloonix_ssh ${NET} ${VM_NAME} "cat > /etc/apt/sources.list << EOF
deb ${MIRROR} ${DIST} main contrib non-free
EOF"

cloonix_cli ${NET} add lan ${VM_NAME} 0 lan_nat
cloonix_ssh ${NET} ${VM_NAME} "dhclient eth0"
cloonix_ssh ${NET} ${VM_NAME} "apt-get --assume-yes update"
cloonix_ssh ${NET} ${VM_NAME} "apt-get -y install ${PKT_LIST}"
cloonix_cli ${NET} cnf kvm halt ${VM_NAME}
cloonix_cli ${NET} del sat nat
echo
echo ${VM_NAME}.qcow2 ready
echo
sleep 1
}
#----------------------------------------------------------------------



