#!/bin/bash
# Build a Rocky Linux GNOME desktop VM from rocky.qcow2.
NET="nemo"
VMNAME="pod"
SRC="rocky.qcow2"
QCOW2="rocky_gnome.qcow2"
BULK="/var/lib/cloonix/bulk"
. "$(dirname "$0")/desk_common.sh"
desk_start
# The base image was left pointing at the mirror as a guest sees it.
desk_ssh "/etc/yum.repos.d of ${SRC} does not name the mirror 10.117.0.1, rebuild ${SRC}" \
  "grep -rq 10.117.0.1 /etc/yum.repos.d"
# Without it the repo urls carry the minor version, and that is a wall of 404.
desk_ssh "the dnf releasever could not be pinned to the major version" \
  "mkdir -p /etc/dnf/vars && . /etc/os-release && \
   echo \${VERSION_ID%%.*} > /etc/dnf/vars/releasever && cat /etc/dnf/vars/releasever"
desk_rpm_no_internet_repo
desk_ssh "dnf install tar failed" "dnf install -y tar"
desk_install "the install of 'Server with GUI' failed" \
  "dnf groupinstall -y 'Server with GUI'" \
  "dnf install -y @graphical-server-environment"
desk_session gnome-session
desk_dm gdm.service
# After the desktop: set-x11-keymap wants the xkb data it brings.
desk_opt "console keymap fr not set" "localectl set-keymap fr"
desk_opt "x11 keymap fr not set" "localectl set-x11-keymap fr"
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine. chromium lives in epel.
desk_opt "no firefox in the image" "dnf install -y firefox"
desk_opt "no chromium in the image, blink is untested on it" "dnf install -y chromium"
desk_opt "no epiphany in the image" "dnf install -y epiphany"
desk_ssh "gdm autologin could not be written" \
  "mkdir -p /etc/gdm && cat > /etc/gdm/custom.conf << EOF
[daemon]
AutomaticLoginEnable=true
AutomaticLogin=user
EOF"
desk_disable NetworkManager \
             bluetooth.service \
             wpa_supplicant.service \
             remote-fs.target \
             systemd-network-generator.service \
             systemd-networkd-wait-online.service \
             systemd-pstore.service \
             fstrim.timer \
             logrotate.timer
desk_ssh "resolv.conf could not be removed" "rm -f /etc/resolv.conf"
desk_enable_if_present systemd-networkd.service
desk_stop
