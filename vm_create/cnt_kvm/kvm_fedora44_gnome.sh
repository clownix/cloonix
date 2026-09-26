#!/bin/bash
# Build a Fedora 44 GNOME desktop VM from fedora44.qcow2.
NET="nemo"
VMNAME="pod"
SRC="fedora44.qcow2"
QCOW2="fedora44_gnome.qcow2"
BULK="/var/lib/cloonix/bulk"
. "$(dirname "$0")/desk_common.sh"
desk_start
# The base image was left pointing at the mirror as a guest sees it.
desk_ssh "/etc/yum.repos.d of ${SRC} does not name the mirror 10.117.0.1, rebuild ${SRC}" \
  "grep -rq 10.117.0.1 /etc/yum.repos.d"
desk_rpm_no_internet_repo
desk_ssh "dnf5 install tar failed" "dnf5 install -y tar"
desk_ssh "dnf5 install kernel-modules-internal failed" \
  "dnf5 install -y kernel-modules-internal.x86_64"
desk_install "the install of @gnome-desktop failed" \
  "dnf5 install -y @gnome-desktop" \
  "dnf5 install -y @workstation-product-environment"
desk_session gnome-session
desk_dm gdm.service
# After the desktop: set-x11-keymap wants the xkb data it brings.
desk_opt "console keymap fr not set" "localectl set-keymap fr"
desk_opt "x11 keymap fr not set" "localectl set-x11-keymap fr"
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "dnf5 install -y firefox"
desk_opt "no chromium in the image" "dnf5 install -y chromium"
desk_opt "no epiphany in the image" "dnf5 install -y epiphany"
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
