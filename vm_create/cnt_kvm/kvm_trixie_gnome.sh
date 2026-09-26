#!/bin/bash
# Build a Debian trixie GNOME desktop VM from trixie.qcow2.
NET="nemo"
VMNAME="pod"
SRC="trixie.qcow2"
QCOW2="trixie_gnome.qcow2"
BULK="/var/lib/cloonix/bulk"
# The mirror holds one directory per distro at its root, and the trixie one is
# complete: contrib, non-free and non-free-firmware included.
REPO="http://10.117.0.1/trixie"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"
. "$(dirname "$0")/desk_common.sh"
desk_start
desk_ssh "sources.list could not be written" "cat > /etc/apt/sources.list << EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} trixie main contrib non-free non-free-firmware
EOF"
desk_ssh "apt-get update failed" "${APT} update"
desk_install "the install of gnome failed" "${APT} install gnome rsyslog"
desk_session gnome-session
# gdm3 enables itself at install.
desk_dm
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "${APT} install --no-install-recommends firefox-esr"
desk_opt "no chromium in the image" "${APT} install --no-install-recommends chromium"
desk_opt "no epiphany in the image" "${APT} install --no-install-recommends epiphany-browser"
desk_disable NetworkManager \
             networking.service \
             bluetooth.service \
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
desk_stop
