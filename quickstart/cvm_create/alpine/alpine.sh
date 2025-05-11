#!/bin/bash
#----------------------------------------------------------------------------#
HERE=`pwd`
NET="nemo"
NAME="alpine"
ALPINE0="/var/lib/cloonix/bulk/alpine0"
ALPINE="/var/lib/cloonix/bulk/alpine"
#----------------------------------------------------------------------------#
if [ ! -e ${ALPINE0} ]; then
  echo ${ALPINE0} does not exist!
  exit 1
fi
#----------------------------------------------------------------------------#
if [ -e ${ALPINE} ]; then
  echo ${ALPINE} already exists, please erase
  exit 1
fi
#----------------------------------------------------------------------------#
wget --delete-after http://127.0.0.1/alpine/main 1>/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo http://127.0.0.1/alpine/main not reachable
  exit 1
fi
#----------------------------------------------------------------------#
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ ! -z "${is_started}" ]; then
  echo cloonix $NET working! Please kill it with:
  echo cloonix_cli $NET kil
  exit 1
fi
#----------------------------------------------------------------------#
unshare --user --fork --pid --mount --mount-proc \
        --map-users=100000,0,10000 --map-groups=100000,0,10000 \
        --setuid 0 --setgid 0 -- \
        /bin/bash -c "cp -rf ${ALPINE0} ${ALPINE}"
#----------------------------------------------------------------------#
cloonix_net ${NET}
cloonix_gui ${NET}
cloonix_cli ${NET} add cvm ${NAME} eth=s alpine --persistent
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
cloonix_ssh ${NET} ${NAME} "dhcpcd eth0"
#----------------------------------------------------------------------------#
LIST="xfce4 xfce4-terminal xfce4-session xeyes rsyslog dbus dbus-x11 strace"
LIST="${LIST} bash vim font-dejavu"

apkcmd="apk --allow-untrusted --no-cache --update add"

for i in ${LIST}; do
  echo DOING: ${i}
  cloonix_ssh ${NET} ${NAME} "${apkcmd} ${i}"
done

LIST="syslog dbus"
for i in ${LIST}; do
  echo DOING: "rc-update add ${i}"
  cloonix_ssh ${NET} ${NAME} "rc-update add ${i}"
done

#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} 'cat > /usr/bin/cloonix_lxsession.sh << EOF
#!/bin/bash

for i in tumblerd \
         xfdesktop \
         Thunar \
         xfsettingsd \
         xfwm4 \
         ssh-agent \
         dbus-daemon \
         dbus-launch \
         xfce4 ; do
  LIST=\$(ps axo pid,args | grep \${i} | grep -v grep | awk "{print \\\$1}")
  echo \${i} \${LIST}
  kill -9 \${LIST}
done
rm -rf /root/.cache
rm -rf /root/.dbus/
/usr/bin/xfce4-session
EOF'
cloonix_ssh ${NET} ${NAME} "chmod +x /usr/bin/cloonix_lxsession.sh"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/at-spi-dbus-bus.desktop"
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/gnome-keyring-pkcs11.desktop"
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/gnome-keyring-secrets.desktop"
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/gnome-keyring-ssh.desktop"
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/xfce4-power-manager.desktop"
cloonix_ssh ${NET} ${NAME} "rm -f /etc/xdg/autostart/xscreensaver.desktop"
#----------------------------------------------------------------------------#
cloonix_ssh ${NET} ${NAME} "sync"
cloonix_ssh ${NET} ${NAME} "poweroff"
cloonix_cli ${NET} kil
#----------------------------------------------------------------------------#

