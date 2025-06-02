#!/bin/bash
#----------------------------------------------------------------------#
HERE=`pwd`
DISTRO="bookworm"
BOOKWORM0="/var/lib/cloonix/bulk/bookworm0"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/bookworm"
CVMREPO="http://172.17.0.2/debian/bookworm"
MMDEBSTRAP="/usr/bin/mmdebstrap"
#----------------------------------------------------------------------#
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
#----------------------------------------------------------------------#
if [ -e $BOOKWORM0 ]; then
  echo $BOOKWORM0 already exists, please delete
  exit 1
fi
#----------------------------------------------------------------------#
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#-----------------------------------------------------------------------#
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${BOOKWORM0}" \
        -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
#----------------------------------------------------------------------#
LIST="iputils-ping,net-tools,procps,iproute2,systemd,dbus,strace,vim"
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         ${MMDEBSTRAP} --variant=minbase \
               --include=${LIST} \
               ${DISTRO} ${BOOKWORM0} ${REPO}"
#----------------------------------------------------------------------#
cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CVMREPO} ${DISTRO} main
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='cat > /sbin/cloonix_agent.sh << "EOF"
#!/bin/bash
/cloonixmnt/cnf_fs/cloonix-agent
/cloonixmnt/cnf_fs/cloonix-dropbear-sshd
if [ -e /cloonixmnt/cnf_fs/startup_nb_eth ]; then
  NB_ETH=$(cat /cloonixmnt/cnf_fs/startup_nb_eth)
  NB_ETH=$((NB_ETH-1))
  for i in $(seq 0 $NB_ETH); do
    LIST="${LIST} eth$i"
  done
  for eth in $LIST; do
    VAL=$(ip link show | grep ": ${eth}@")
    while [  $(ip link show | egrep -c ": ${eth}") = 0 ]; do
      sleep 1
    done
  done
fi
EOF'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='chmod +x /sbin/cloonix_agent.sh'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='cat > /etc/systemd/system/cloonix_agent.service << EOF
[Unit]
  After=rsyslog.service
[Service]
  TimeoutStartSec=infinity
  Type=forking
  RemainAfterExit=y
  ExecStart=/sbin/cloonix_agent.sh
[Install]
  WantedBy=multi-user.target
EOF'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='systemctl enable cloonix_agent'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cat > ${BOOKWORM0}/bin/cloonixsbininit << EOF
exec &> /tmp/cloonixsbininit.log
echo STARTING
cd /sbin
ln -s /lib/systemd/systemd init
exec /sbin/init 
EOF
#-----------------------------------------------------------------------#
chmod 755 ${BOOKWORM0}/bin/cloonixsbininit
#-----------------------------------------------------------------------#
rm -f ${BOOKWORM0}/usr/bin/sh
cd ${BOOKWORM0}/usr/bin
ln -s /usr/bin/bash sh
cd ${HERE} 
for i in "dev" "proc" "sys"; do
  chmod 755 ${BOOKWORM0}/${i}
done
mkdir -p ${ALPINE0}/lib/modules
chmod 777 ${BOOKWORM0}/root
#-----------------------------------------------------------------------#



