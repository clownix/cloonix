#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
#----------------------------------------------------------------------#
NET="nemo"
VMNAME="novnc"
FROM="bookworm_lxde.qcow2"
QCOW2="bookworm_novnc.qcow2"
REPO="http://172.17.0.2/debian/bookworm"
#----------------------------------------------------------------------#
set +e
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
cloonix_net ${NET}
sleep 2
set -e
#----------------------------------------------------------------------#
cp -vf /var/lib/cloonix/bulk/${FROM} /var/lib/cloonix/bulk/${QCOW2}
sync
#-----------------------------------------------------------------------#
PARAMS="ram=3000 cpu=2 eth=v"
cloonix_cli ${NET} add kvm ${VMNAME} ${PARAMS} ${QCOW2} --persistent
#-----------------------------------------------------------------------#
while ! cloonix_ssh ${NET} ${VMNAME} "echo"; do
  echo ${VMNAME} not ready, waiting 3 sec
  sleep 3
done
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} bookworm main contrib non-free non-free-firmware
EOF"
#-----------------------------------------------------------------------#
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan nat 0 lan_nat
cloonix_cli ${NET} add lan ${VMNAME} 0 lan_nat
cloonix_ssh ${NET} ${VMNAME} "dhclient eth0"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "apt-get --assume-yes install \
                              xvfb x11vnc wm2 python3-websockify \
                              novnc"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "cat > /usr/bin/cloonix_novnc.sh << EOF
#!/bin/bash
pkill websockify
pkill x11vnc
pkill xeyes
pkill Xvfb
openssl req -x509 -nodes -newkey rsa:3072 -keyout novnc.pem -subj /CN=cloonix.net -out /root/novnc.pem -days 3650
export DISPLAY=:33
Xvfb :33 -ac &
sleep 1
wm2 &
xeyes &
x11vnc -N -nopw -localhost -display :33 &
websockify -D --web=/usr/share/novnc/ --cert=/root/novnc.pem  6080 localhost:5933
sleep 3000
EOF"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "chmod +x /usr/bin/cloonix_novnc.sh"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
cloonix_ssh ${NET} ${VMNAME} "sync"
sleep 1
set +e
cloonix_cli ${NET} kil
sleep 1
echo END
#-----------------------------------------------------------------------#

