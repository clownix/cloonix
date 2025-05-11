#!/bin/bash
#----------------------------------------------------------------------#
DISTRO="bookworm"
BOOKWORM0="/var/lib/cloonix/bulk/bookworm0"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/bookworm"
CVMREPO="http://172.17.0.2/debian/bookworm"
DEBOOTSTRAP="/usr/sbin/debootstrap"
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
  unshare --user --fork --pid --mount --mount-proc \
          --map-users=100000,0,10000 --map-groups=100000,0,10000 \
          --setuid 0 --setgid 0 --wd "${BOOKWORM0}" -- \
          /bin/bash -c " \
                   mount -t proc none proc ; \
                   mount -o rw,bind /dev/zero dev/zero ; \
                   mount -o rw,bind /dev/null dev/null ; \
                   /sbin/chroot . /bin/bash -c '$1'"
}
#----------------------------------------------------------------------#
unshare --user --fork --pid --mount --mount-proc \
        --map-users=100000,0,10000 --map-groups=100000,0,10000 \
        --setuid 0 --setgid 0 -- \
        /bin/bash -c "\
                      mkdir -p ${BOOKWORM0}/dev ;\
                      mkdir -p ${BOOKWORM0}/proc"
#----------------------------------------------------------------------#
unshare --user --fork --pid --mount --mount-proc \
        --map-users=100000,0,10000 --map-groups=100000,0,10000 \
        --setuid 0 --setgid 0 --wd "${BOOKWORM0}" -- \
        /bin/bash -c "\
                      touch dev/zero ; \
                      touch dev/null ; \
                      mount -t proc none proc ; \
                      mount -o rw,bind /dev/zero dev/zero ; \
                      mount -o rw,bind /dev/null dev/null ; \
                      ${DEBOOTSTRAP} --no-check-certificate \
                                     --variant=minbase \
                                     --include=systemd,dhcpcd-base \
                                     --no-check-gpg \
                                     --arch amd64 \
                                     ${DISTRO} \
                                     ${BOOKWORM0} \
                                     ${REPO}"
#-----------------------------------------------------------------------#

cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CVMREPO} ${DISTRO} main
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd="passwd root <<EOF
root
root
EOF"
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#
cmd='cat > /sbin/cloonix_agent.sh << "EOF"
#!/bin/bash
export LD_LIBRARY_PATH="/mnt/cloonix_config_fs/lib"
/mnt/cloonix_config_fs/cloonix-agent
/mnt/cloonix_config_fs/cloonix-dropbear-sshd
unset LD_LIBRARY_PATH
if [ -e /mnt/cloonix_config_fs/startup_nb_eth ]; then
  NB_ETH=$(cat /mnt/cloonix_config_fs/startup_nb_eth)
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
cmd='ln -s /lib/systemd/systemd /sbin/init'
fct_unshare_chroot "${cmd}"
#-----------------------------------------------------------------------#



