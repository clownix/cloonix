#!/bin/bash
# Build an Arch Linux KDE Plasma desktop VM from arch.qcow2.
NET="nemo"
VMNAME="pod"
SRC="arch.qcow2"
QCOW2="arch_plasma.qcow2"
BULK="/var/lib/cloonix/bulk"
PAC="pacman --noconfirm"
. "$(dirname "$0")/desk_common.sh"
desk_start
# The base image was left pointing at the mirror as a guest sees it.
desk_ssh "/etc/pacman.d/mirrorlist of ${SRC} does not name the mirror 10.117.0.1, rebuild ${SRC}" \
  "grep -rq 10.117.0.1 /etc/pacman.d/mirrorlist"
desk_ssh "pacman -Sy failed" "${PAC} -Sy"
desk_install "the install of plasma-meta sddm xorg-server failed" \
  "${PAC} -S plasma-meta sddm xorg-server" \
  "${PAC} -S plasma sddm xorg-server"
desk_session startplasma-wayland startplasma-x11
desk_dm sddm.service
# After the desktop: set-x11-keymap wants the xkb data it brings.
desk_opt "console keymap fr not set" "localectl set-keymap fr"
desk_opt "x11 keymap fr not set" "localectl set-x11-keymap fr"
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "${PAC} -S firefox"
desk_opt "no chromium in the image" "${PAC} -S chromium"
desk_opt "no epiphany in the image" "${PAC} -S epiphany"
desk_ssh "sddm autologin could not be written" \
  "mkdir -p /etc/sddm.conf.d && cat > /etc/sddm.conf.d/autologin.conf << EOF
[Autologin]
User=user
Session=plasma
Relogin=false
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
