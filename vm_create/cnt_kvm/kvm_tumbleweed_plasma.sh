#!/bin/bash
# Build an openSUSE Tumbleweed KDE Plasma desktop VM from tumbleweed.qcow2.
NET="nemo"
VMNAME="pod"
SRC="tumbleweed.qcow2"
QCOW2="tumbleweed_plasma.qcow2"
BULK="/var/lib/cloonix/bulk"
# dnf, not zypper: it is what kvm_tumbleweed.sh builds with.
DNF="dnf --releasever=tumbleweed --nogpgcheck --setopt=sslverify=0"
# cloonix-oss: the id rpm_local_repo_only of kvm_tumbleweed.sh gave the mirror.
DNF+=" --disablerepo=* --enablerepo=cloonix-oss"
LEAN_INST="${DNF} --setopt=install_weak_deps=False -y install"
# The pattern holds its display manager and most of the desktop as Recommends.
DESK_INST="${DNF} --setopt=install_weak_deps=True -y install"
. "$(dirname "$0")/desk_common.sh"
desk_start
desk_ssh "no dnf in the base image" "command -v dnf"
# The base image was left pointing at the mirror as a guest sees it.
desk_ssh "/etc/yum.repos.d of ${SRC} does not name the mirror 10.117.0.1, rebuild ${SRC}" \
  "grep -rq 10.117.0.1 /etc/yum.repos.d"
desk_ssh "dnf makecache failed" "${DNF} makecache"
# By name: the pattern only recommends sddm-qt6 ('sddm' is both flavours),
# and nothing requires the X server the sddm greeter runs on.
desk_install "the install of the plasma pattern failed" \
  "${DESK_INST} patterns-kde-kde_plasma sddm-qt6 xorg-x11-server"
desk_session startplasma-wayland startplasma-x11
# display-manager.service is the opensuse one, it starts the sddm alternative.
desk_dm display-manager.service sddm.service plasmalogin.service
desk_opt "console keymap fr not set" "localectl set-keymap fr"
desk_opt "x11 keymap fr not set" "localectl set-x11-keymap fr"
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "${LEAN_INST} MozillaFirefox"
desk_opt "no chromium in the image" "${LEAN_INST} chromium"
desk_opt "no epiphany in the image" "${LEAN_INST} epiphany"
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
             logrotate.timer \
             btrfs-balance.timer \
             btrfs-scrub.timer \
             snapper-cleanup.timer \
             snapper-timeline.timer
desk_ssh "resolv.conf could not be removed" "rm -f /etc/resolv.conf"
desk_stop
