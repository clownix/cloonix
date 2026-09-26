#!/bin/bash
# Build a Debian trixie LXDE desktop VM from trixie.qcow2.
NET="nemo"
VMNAME="pod"
DISTRO="trixie"
SRC="${DISTRO}.qcow2"
QCOW2="${DISTRO}_lxde.qcow2"
BULK="/var/lib/cloonix/bulk"
REPO="http://10.117.0.1/${DISTRO}"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"
. "$(dirname "$0")/desk_common.sh"
desk_start
desk_ssh "sources.list could not be written" "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main contrib non-free non-free-firmware
EOF"
desk_ssh "apt-get update failed" "${APT} update"
desk_install "the install of lxde failed" \
  "${APT} install openssh-client openssh-server binutils rsyslog lxde"
desk_session startlxde
# lightdm enables itself at install.
desk_dm
# xset of the autostart below: lxde only recommends it.
desk_ssh "the install of x11-xserver-utils failed" \
  "${APT} install --no-install-recommends x11-xserver-utils"
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "${APT} install --no-install-recommends firefox-esr"
desk_opt "no chromium in the image" "${APT} install --no-install-recommends chromium"
desk_opt "no epiphany in the image" "${APT} install --no-install-recommends epiphany-browser"
desk_ssh "xscreensaver could not be removed" \
  "dpkg -s xscreensaver >/dev/null 2>&1 || exit 0; ${APT} remove xscreensaver"
desk_ssh "lightdm autologin could not be written" "cat >> /etc/lightdm/lightdm.conf << EOF
[SeatDefaults]
autologin-in-background=true
autologin-user=user
autologin-user-timeout=5
EOF"
desk_ssh "the lxde autostart could not be written" "cat > /etc/xdg/lxsession/LXDE/autostart << EOF
@lxpanel --profile LXDE
@pcmanfm --desktop --profile LXDE
@xset s noblank
@xset s off
@xset -dpms
EOF"
desk_disable networking.service \
             e2scrub_reap.service \
             wpa_supplicant.service \
             remote-fs.target \
             apt-daily.timer \
             apt-daily-upgrade.timer \
             systemd-network-generator.service \
             systemd-networkd-wait-online.service \
             systemd-pstore.service \
             dpkg-db-backup.timer \
             e2scrub_all.timer \
             fstrim.timer \
             logrotate.timer \
             man-db.timer
desk_ssh "resolv.conf could not be removed" "rm -f /etc/resolv.conf"
desk_enable_if_present systemd-networkd.service
desk_stop
