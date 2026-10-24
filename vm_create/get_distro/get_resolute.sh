#!/bin/bash

DISTRO="resolute"
TARGET="/media/perrier/T7Shield2/distros"
cible="${TARGET}/${DISTRO}"
export GNUPGHOME="${TARGET}/mirrorkeyring"
/usr/bin/debmirror -a amd64 \
                   --no-source \
                   -s main,universe,multiverse \
                   -h fr.archive.ubuntu.com \
                   -d ${DISTRO},${DISTRO}-security,${DISTRO}-updates \
                   -r /ubuntu \
                   --no-check-gpg \
                   --progress \
                   --method=http \
                   --exclude='google-android.*' \
                   $cible
