#!/bin/bash
#----------------------------------------------------------------------------#
NET="nemo"
NAME="bookworm"
DEFVIM="/usr/share/vim/vim90/defaults.vim"
REPO="http://deb.debian.org/debian"
REPO="http://127.0.0.1/debian/bookworm"
BOOKWORM0="/var/lib/cloonix/bulk/bookworm0"
BOOKWORM="/var/lib/cloonix/bulk/bookworm"
#----------------------------------------------------------------------------#
if [ ! -e ${BOOKWORM0} ]; then
  echo ${BOOKWORM0} does not exist!
  exit 1
fi
#----------------------------------------------------------------------------#
if [ -e ${BOOKWORM} ]; then
  echo ${BOOKWORM} already exists, please erase
  exit 1
fi
#----------------------------------------------------------------------#
wget --delete-after ${REPO} 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
#-----------------------------------------------------------------------#
cp -rf ${BOOKWORM0} ${BOOKWORM}
sync
sleep 5
sync
#----------------------------------------------------------------------#
cloonix_net ${NET}
cloonix_gui ${NET}
cloonix_cli ${NET} add cvm ${NAME} eth=s bookworm --persistent
cloonix_cli ${NET} add nat nat
cloonix_cli ${NET} add lan ${NAME} 0 lan1
cloonix_cli ${NET} add lan nat 0 lan1
#----------------------------------------------------------------------------#
set +e
while ! cloonix_ssh ${NET} ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 3 sec
  sleep 3
done
set -e
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "ip addr add dev eth0 172.17.0.12/24"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes update"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "DEBIAN_FRONTEND=noninteractive \
                      DEBCONF_NONINTERACTIVE_SEEN=true \
                      apt-get -o Dpkg::Options::=--force-confdef \
                      -o Acquire::Check-Valid-Until=false \
                      --allow-unauthenticated --assume-yes install \
                      openssh-client vim zstd bash-completion \
                      net-tools qemu-guest-agent systemd \
                      x11-apps iperf3 xtrace strace rsyslog \
                      lsof xauth keyboard-configuration file \
                      locales console-data console-setup kbd \
                      accountsservice lxde openvswitch-switch \
                      virt-manager vlan sshpass curl xserver-xspice"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "/sbin/useradd --create-home --shell /bin/bash user"
cloonix_ssh ${NET} ${NAME} "passwd user << EOF
user
user
EOF"
cloonix_ssh ${NET} ${NAME} "/sbin/usermod -aG video,render,sudo user"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "systemctl enable rsyslog"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "cat > /root/debconf_selection << EOF
#locales locales_to_be_generated multiselect en_US.UTF-8 UTF-8
#locales locales/default_environment_locale select en_US.UTF-8
locales locales_to_be_generated multiselect fr_FR.UTF-8 UTF-8
locales locales/default_environment_locale select fr_FR.UTF-8
keyboard-configuration keyboard-configuration/layoutcode string fr
keyboard-configuration keyboard-configuration/variant select Français - Français (variante)
keyboard-configuration keyboard-configuration/model select Generic 105-key (Intl) PC
keyboard-configuration keyboard-configuration/xkb-keymap select fr(latin9)
EOF"

cloonix_ssh ${NET} ${NAME} "cat > /etc/default/locale << EOF
LC_CTYPE=\"fr_FR.UTF-8\"
LC_ALL=\"fr_FR.UTF-8\"
LANG=\"fr_FR.UTF-8\"
EOF"

cloonix_ssh ${NET} ${NAME} "cat > /etc/default/keyboard << EOF
XKBMODEL=\"pc105\"
XKBLAYOUT=\"fr\"
XKBVARIANT=\"azerty\"
XKBOPTIONS=\"\"
BACKSPACE=\"guess\"
EOF"

cloonix_ssh ${NET} ${NAME} "sed -i s'/# fr_FR.UTF-8/fr_FR.UTF-8/' /etc/locale.gen"
cloonix_ssh ${NET} ${NAME} "debconf-set-selections /root/debconf_selection"
cloonix_ssh ${NET} ${NAME} "dpkg-reconfigure -f noninteractive locales"
cloonix_ssh ${NET} ${NAME} "rm -f /root/debconf_selection"
#-----------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "sed -i s'/filetype plugin/\"filetype plugin/' $DEFVIM"
cloonix_ssh ${NET} ${NAME} "sed -i s'/set mouse/\"set mouse/' $DEFVIM"
#-----------------------------------------------------------------------#

#----------------------------------------------------------------------------#
for i in "bluetooth.service" \
         "connman.service" \
         "connman-wait-online.service" \
         "dundee.service" \
         "ofono.service" \
         "e2scrub_reap.service" \
         "systemd-pstore.service" \
         "wpa_supplicant.service" \
         "remote-fs.target" \
         "apt-daily-upgrade.timer" \
         "apt-daily.timer" \
         "dpkg-db-backup.timer" \
         "fstrim.timer" \
         "systemd-networkd.service" \
         "systemd-network-generator.service" \
         "systemd-networkd-wait-online.service" \
         "bluetooth.service" \
         "logrotate.timer" \
         "man-db.timer" \
         "polkit.service" ; do

  cloonix_ssh ${NET} ${NAME} "systemctl disable ${i}"
done
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "systemctl enable rsyslog.service"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "cat > /etc/xdg/lxsession/LXDE/autostart << EOF
@lxpanel --profile LXDE
@pcmanfm --desktop --profile LXDE
@xset s noblank
@xset s off
@xset -dpms
EOF"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "cat > /etc/xdg/pcmanfm/LXDE/pcmanfm.conf << EOF
[config]
bm_open_method=0
su_cmd=xdg-su -c '%s'

[desktop]
wallpaper_mode=color
desktop_bg=#ffac33
desktop_fg=#ffffff
desktop_shadow=#000000

[ui]
always_show_tabs=0
hide_close_btn=0
win_width=640
win_height=480
view_mode=icon
show_hidden=0
sort=name;ascending;
EOF"
#----------------------------------------------------------------------------#
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} 'cat > /usr/bin/cloonix_lxsession.sh << EOF
#!/bin/bash
LXPID=\$(ps axo pid,ppid,args | grep /usr/bin/lxsession | grep -v grep | awk "{print \\\$1}")
LXPPID=\$(ps axo pid,ppid,args | grep /usr/bin/lxsession | grep -v grep | awk "{print \\\$2}")

if [ ! -z \$LXPID ]; then

CHILD=\$(ps --ppid \$LXPID | grep openbox |  awk "{print \\\$1}")
echo kill openbox \$CHILD
kill \$CHILD

CHILD=\$(ps --ppid \$LXPID | grep lxpanel |  awk "{print \\\$1}")
echo kill lxpanel \$CHILD
kill \$CHILD

CHILD=\$(ps --ppid \$LXPID | grep pcmanfm |  awk "{print \\\$1}")
echo kill pcmanfm \$CHILD
kill \$CHILD

echo kill lxsession \$LXPID
kill \$LXPID

echo kill cloonix-dropbear-sshd \$LXPPID
kill \$LXPPID

CHILD=\$(ps axo pid,args | grep /usr/lib/menu-cache/menu-cached | grep -v grep | awk "{print \\\$1}")
echo kill menu-cached \$CHILD
kill \$CHILD

CHILD=\$(ps axo pid,args | grep /usr/bin/ssh-agent | grep -v grep | awk "{print \\\$1}")
echo kill ssh-agent \$CHILD
kill \$CHILD

fi
rm -rf /root/.cache/ menu-cached*
/usr/bin/lxsession
EOF'
cloonix_ssh ${NET} ${NAME} "chmod +x /usr/bin/cloonix_lxsession.sh"
#----------------------------------------------------------------------------#

# desktop_bg=#67c8b0 # Light Green
# desktop_bg=#33d1ff # Light Blue
# desktop_bg=#bb33FF # Purple
# desktop_bg=#ffac33 # Orange
cloonix_ssh ${NET} ${NAME} "rm -rf /etc/xdg/autostart/*"
cloonix_ssh ${NET} ${NAME} "rm -f /usr/bin/lxpolkit"
cloonix_ssh ${NET} ${NAME} "rm -f /usr/lib/polkit-1/polkitd"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "sync"
sleep 3
set +e
cloonix_ssh ${NET} ${NAME} "poweroff"
sleep 5
cloonix_cli ${NET} kil
echo END
#----------------------------------------------------------------------------#

