#!/bin/bash
# Build an Ubuntu resolute GNOME desktop VM from resolute.qcow2.
NET="nemo"
VMNAME="pod"
DISTRO="resolute"
SRC="${DISTRO}.qcow2"
QCOW2="${DISTRO}_gnome.qcow2"
BULK="/var/lib/cloonix/bulk"
# one directory per distro at the mirror root, no /ubuntu level
REPO="http://10.117.0.1/${DISTRO}"
COMPONENTS="main universe multiverse"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"
. "$(dirname "$0")/desk_common.sh"
desk_start
desk_ssh "sources.list could not be written" "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} ${COMPONENTS}
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO}-updates ${COMPONENTS}
EOF"
# snapd pinned out: on ubuntu firefox and chromium-browser are snap transitional
# debs, and snapd retries the store for 30 minutes.
desk_ssh "the snapd pin could not be written" "cat > /etc/apt/preferences.d/nosnap.pref << EOF
Package: snapd
Pin: release a=*
Pin-Priority: -10
EOF"
desk_ssh "snapd could not be masked" \
  "systemctl mask snapd.service snapd.socket snapd.seeded.service"
desk_ssh "apt-get update failed" "${APT} update"
# gnome-core rather than ubuntu-desktop-minimal, which wants snapd.
desk_install "the install of gnome-core gdm3 failed" \
  "${APT} install gnome-core gdm3 rsyslog" \
  "${APT} install ubuntu-desktop-minimal rsyslog"
desk_session gnome-session
desk_dm gdm3.service
desk_opt "console keymap fr not set" "localectl set-keymap fr"
desk_opt "x11 keymap fr not set" "localectl set-x11-keymap fr"
# Only epiphany (webkitgtk) is a real deb on ubuntu: firefox and chromium are
# snaps, gecko and blink are covered by the other images.
desk_opt "no epiphany in the image" "${APT} install --no-install-recommends epiphany-browser"
desk_ssh "gdm autologin could not be written" \
  "mkdir -p /etc/gdm3 && cat > /etc/gdm3/custom.conf << EOF
[daemon]
AutomaticLoginEnable=true
AutomaticLogin=user
EOF"
desk_disable NetworkManager \
             networking.service \
             wpa_supplicant.service \
             bluetooth.service \
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
