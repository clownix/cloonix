#!/bin/bash
#----------------------------------------------------------------------------#
NET="nemo"
CVM="Cnt1"
#----------------------------------------------------------------------------#
set +e
if [ ! -e "/var/lib/cloonix/bulk/cvm_rootfs_orig" ]; then
  echo /var/lib/cloonix/bulk/cvm_rootfs_orig does not exist!
  exit 1
fi

sudo rm -rf /var/lib/cloonix/bulk/cvm_rootfs
sudo cp -rf /var/lib/cloonix/bulk/cvm_rootfs_orig /var/lib/cloonix/bulk/cvm_rootfs

is_started=$(cloonix_cli ${NET} pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli ${NET} kil
  echo waiting 20 sec for cleaning
  sleep 20
fi
#----------------------------------------------------------------------------#
cloonix_net ${NET}
sleep 2
cloonix_gui ${NET}
set -e
#----------------------------------------------------------------------------#
cloonix_cli ${NET} add cvm ${CVM} eth=s cvm_rootfs --persistent
while ! cloonix_ssh ${NET} ${CVM} "echo"; do
  echo ${CVM} not ready, waiting 3 sec
  sleep 3
done
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

  cloonix_ssh ${NET} ${CVM} "systemctl disable ${i}"
done
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "systemctl enable rsyslog.service"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "cat > /etc/xdg/lxsession/LXDE/autostart << EOF
@lxpanel --profile LXDE
@pcmanfm --desktop --profile LXDE
@xset s noblank
@xset s off
@xset -dpms
EOF"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "cat > /etc/xdg/pcmanfm/LXDE/pcmanfm.conf << EOF
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
cloonix_ssh ${NET} ${CVM} 'cat > /usr/bin/cloonix_lxsession.sh << EOF
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
cloonix_ssh ${NET} ${CVM} "chmod +x /usr/bin/cloonix_lxsession.sh"
#----------------------------------------------------------------------------#

# desktop_bg=#67c8b0 # Light Green
# desktop_bg=#33d1ff # Light Blue
# desktop_bg=#bb33FF # Purple
# desktop_bg=#ffac33 # Orange
cloonix_ssh ${NET} ${CVM} "rm -rf /etc/xdg/autostart/*"
cloonix_ssh ${NET} ${CVM} "rm -f /usr/bin/lxpolkit"
cloonix_ssh ${NET} ${CVM} "rm -f /usr/lib/polkit-1/polkitd"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${CVM} "sync"
sleep 2
set +e
cloonix_ssh ${NET} ${CVM} "poweroff"
sleep 5
cloonix_cli ${NET} kil
echo END
#----------------------------------------------------------------------------#

