#!/bin/bash
# Build an Alpine XFCE desktop VM from alpine.qcow2.
NET="nemo"
VMNAME="pod"
SRC="alpine.qcow2"
QCOW2="alpine_xfce.qcow2"
BULK="/var/lib/cloonix/bulk"
REPO="http://10.117.0.1/alpine"
. "$(dirname "$0")/desk_common.sh"
desk_start
desk_ssh "the apk repositories could not be written" "cat > /etc/apk/repositories << EOF
${REPO}/main
${REPO}/community
EOF"
desk_ssh "apk update failed" "apk update"
desk_install "the install of xfce failed" \
  "apk add --no-cache xorg-server xf86-input-libinput xf86-video-qxl \
                      eudev dbus \
                      xfce4 xfce4-terminal xfce4-screensaver \
                      lightdm-gtk-greeter setxkbmap"
desk_session startxfce4
# Browsers, one per engine (firefox gecko, chromium blink, epiphany webkitgtk):
# the canvas behaves per engine.
desk_opt "no firefox in the image" "apk add --no-cache firefox"
desk_opt "no chromium in the image" "apk add --no-cache chromium"
desk_opt "no epiphany in the image" "apk add --no-cache epiphany"
# OpenRC equivalent of graphical.target: udev, dbus, lightdm.
desk_ssh "dbus and lightdm could not be added to the default runlevel" \
  "rc-update add dbus default && rc-update add lightdm default && \
   rc-update show default | grep -q lightdm"
desk_opt "a udev service could not be added to the default runlevel, named above" \
  "rc=0; for s in udev udev-trigger udev-settle; do \
     rc-update add \${s} default || { echo \"rc-update add \${s} failed\"; rc=1; }; \
   done; exit \${rc}"
desk_ssh "lightdm autologin could not be set" \
  "sed -i -e 's/^#*autologin-user=.*/autologin-user=user/' \
          -e 's/^#*autologin-session=.*/autologin-session=xfce/' /etc/lightdm/lightdm.conf && \
   { grep -q '^autologin-user=user' /etc/lightdm/lightdm.conf || \
     printf '[Seat:*]\nautologin-user=user\nautologin-session=xfce\n' >> /etc/lightdm/lightdm.conf; }"
desk_ssh "user could not be put in the autologin group" \
  "grep -q '^autologin:' /etc/group || addgroup autologin; \
   addgroup user autologin; grep '^autologin:' /etc/group | grep -qw user"
# Alpine has no localectl: written where xorg reads it.
desk_ssh "the french keymap could not be written" \
  "mkdir -p /etc/X11/xorg.conf.d && cat > /etc/X11/xorg.conf.d/00-keyboard.conf << EOF
Section \\\"InputClass\\\"
    Identifier \\\"system-keyboard\\\"
    MatchIsKeyboard \\\"on\\\"
    Option \\\"XkbLayout\\\" \\\"fr\\\"
EndSection
EOF"
desk_stop
