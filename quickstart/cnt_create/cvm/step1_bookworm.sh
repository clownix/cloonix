#!/bin/bash
#----------------------------------------------------------------------#
DISTRO="bookworm"
ROOTFS="/var/lib/cloonix/bulk/cvm_rootfs_orig"
DEFVIM="${ROOTFS}/usr/share/vim/vim90/defaults.vim"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/bookworm"
CVMREPO="http://172.17.0.2/debian/bookworm"
#----------------------------------------------------------------------#
if [ $UID != 0 ]; then
  echo you must be root
  exit -1
fi
if [ -e $ROOTFS ]; then
  echo $ROOTFS already exists, DELETING
  rm -rf /var/lib/cloonix/bulk/cvm_rootfs
fi
mkdir -p $ROOTFS
#-----------------------------------------------------------------------#
list_pkt="openssh-client,vim,zstd,bash-completion,"
list_pkt+="net-tools,qemu-guest-agent,systemd,"
list_pkt+="x11-apps,iperf3,xtrace,strace,rsyslog,"
list_pkt+="lsof,xauth,keyboard-configuration,file,"
list_pkt+="locales,console-data,console-setup,"
list_pkt+="kbd"

debootstrap --no-check-certificate \
	    --no-check-gpg \
            --arch amd64 \
            --include=$list_pkt \
            ${DISTRO} \
            ${ROOTFS} ${REPO}
#-----------------------------------------------------------------------#
cat > ${ROOTFS}/etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${CVMREPO} ${DISTRO} main
EOF
#-----------------------------------------------------------------------#
chroot ${ROOTFS} passwd root << EOF
root
root
EOF
#-----------------------------------------------------------------------#
chroot ${ROOTFS} useradd --create-home --shell /bin/bash user 
chroot ${ROOTFS} passwd user << EOF
user
user
EOF
chroot ${ROOTFS} usermod -aG video,render,sudo user
#-----------------------------------------------------------------------#
chroot ${ROOTFS} systemctl enable rsyslog
#-----------------------------------------------------------------------#
sed -i s"/filetype plugin/\"filetype plugin/" $DEFVIM
sed -i s"/set mouse/\"set mouse/" $DEFVIM
#-----------------------------------------------------------------------#
cat > ${ROOTFS}/root/debconf_selection << EOF
#locales locales_to_be_generated multiselect en_US.UTF-8 UTF-8
#locales locales/default_environment_locale select en_US.UTF-8
locales locales_to_be_generated multiselect fr_FR.UTF-8 UTF-8
locales locales/default_environment_locale select fr_FR.UTF-8
keyboard-configuration keyboard-configuration/layoutcode string fr
keyboard-configuration keyboard-configuration/variant select Français - Français (variante)
keyboard-configuration keyboard-configuration/model select Generic 105-key (Intl) PC
keyboard-configuration keyboard-configuration/xkb-keymap select fr(latin9)
EOF

cat > ${ROOTFS}/etc/default/locale << EOF
LC_CTYPE="fr_FR.UTF-8"
LC_ALL="fr_FR.UTF-8"
LANG="fr_FR.UTF-8"
EOF

cat > ${ROOTFS}/etc/default/keyboard << EOF
XKBMODEL="pc105"
XKBLAYOUT="fr"
XKBVARIANT="azerty"
XKBOPTIONS=""
BACKSPACE="guess"
EOF

sed -i s"/# fr_FR.UTF-8/fr_FR.UTF-8/" ${ROOTFS}/etc/locale.gen
export DEBCONF_NONINTERACTIVE_SEEN=true
chroot ${ROOTFS} debconf-set-selections /root/debconf_selection
chroot ${ROOTFS} dpkg-reconfigure -f noninteractive locales
rm -f ${ROOTFS}/root/debconf_selection
#-----------------------------------------------------------------------#
cat > ${ROOTFS}/sbin/cloonix_agent.sh << "EOF"
#!/bin/sh
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
EOF
#-----------------------------------------------------------------------#
cat > ${ROOTFS}/etc/systemd/system/cloonix_agent.service << EOF
[Unit]
  After=rsyslog.service
[Service]
  TimeoutStartSec=infinity
  Type=forking
  RemainAfterExit=y
  ExecStart=/sbin/cloonix_agent.sh
[Install]
  WantedBy=multi-user.target
EOF
#-----------------------------------------------------------------------#
chmod +x ${ROOTFS}/sbin/cloonix_agent.sh
chroot ${ROOTFS} systemctl enable cloonix_agent
#-----------------------------------------------------------------------#



