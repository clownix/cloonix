#!/bin/bash
# python_trixie.qcow2: trixie.qcow2 plus python3, for a guest that runs the
# cloonix client itself (demos/cloonix/realnet).
NET="nemo"
DISTRO="trixie"
VMNAME="pod"
SRC="trixie.qcow2"
BULK="/var/lib/cloonix/bulk"
QCOW2="python_${DISTRO}.qcow2"
REPO="http://10.117.0.1/${DISTRO}"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"
[ -e ${BULK}/${SRC} ] || { echo "ERROR: ${BULK}/${SRC} not found, ./kvm_trixie.sh first"; exit 1; }
set +e
# Stop the net and WAIT for it to be gone, then start it and wait until it
# answers -- the lock outlives the process by a second or two.
if cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; then
  echo "a server is up for ${NET}, stopping it"
  cloonix stop ${NET}
fi
tries=0
while cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; do
  tries=$((tries+1))
  [ ${tries} -ge 30 ] && { echo "FATAL: ${NET} will not stop"; exit 1; }
  echo "waiting for ${NET} to stop"
  sleep 2
done
tries=0
while true; do
  cloonix start ${NET} 2>&1 | grep -v "^$"
  sleep 2
  cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server && break
  tries=$((tries+1))
  [ ${tries} -ge 15 ] && { echo "FATAL: ${NET} will not start"; exit 1; }
  echo "server not up yet, retrying"
  sleep 2
done
set -e
cp -vf ${BULK}/${SRC} ${BULK}/${QCOW2}
sync
cloonix cli ${NET} add kvm ${VMNAME} ram=2000 cpu=2 eth=1 ${QCOW2} --persistent
while ! cloonix cli ${NET} ssh ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
cloonix cli ${NET} ssh ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main contrib non-free non-free-firmware
EOF"
cloonix cli ${NET} add nat nat
cloonix cli ${NET} plug nat lan_nat
cloonix cli ${NET} plug ${VMNAME} eth0 lan_nat
set +e
cloonix cli ${NET} ssh ${VMNAME} "ip link set dev eth0 up; \
  ip -4 addr show dev eth0 | grep -q 'inet ' || dhclient -4 eth0; true"
tries=0
while ! cloonix cli ${NET} ssh ${VMNAME} "ping -c1 -W2 10.117.0.1 >/dev/null 2>&1"; do
  tries=$((tries+1))
  [ ${tries} -ge 10 ] && { echo "FATAL: mirror 10.117.0.1 not reachable"; exit 1; }
  echo "network not ready, waiting 3 sec"
  sleep 3
done
set -e
cloonix cli ${NET} ssh ${VMNAME} "${APT} update"
cloonix cli ${NET} ssh ${VMNAME} "${APT} install --no-install-recommends python3"
cloonix cli ${NET} ssh ${VMNAME} "python3 --version"
cloonix cli ${NET} ssh ${VMNAME} "rm -f /etc/resolv.conf"
# Clean stop, verified by the vm leaving the topology: over the agent there is no
# session bus, poweroff on systemd 257 fails, the fallbacks do not need the bus.
set +e
cloonix cli ${NET} ssh ${VMNAME} "sync; sleep 1; sync"
cloonix cli ${NET} ssh ${VMNAME} "poweroff 2>/dev/null || \
  systemctl --force poweroff 2>/dev/null || \
  { sync; echo o > /proc/sysrq-trigger; }"
tries=0
while cloonix cli ${NET} dump commands 2>/dev/null | grep -q " ${VMNAME} "; do
  tries=$((tries+1))
  [ ${tries} -ge 30 ] && { echo "FATAL: ${VMNAME} did not power off, ${QCOW2} is not usable"; exit 1; }
  echo "waiting for ${VMNAME} to power off"
  sleep 2
done
sleep 2
cloonix stop ${NET}
sleep 1
echo "${BULK}/${QCOW2} done"
