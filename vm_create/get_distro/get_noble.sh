#!/bin/bash
# Full noble mirror: four components, four pockets, amd64 binaries, every
# Translation language.
set -u
DISTRO="noble"
DISK="/media/perrier/Samsung_T5"
cible="${DISK}/${DISTRO}"

mountpoint -q "${DISK}" || { echo "FATAL: ${DISK} not mounted"; exit 1; }

/usr/bin/debmirror -a amd64 \
                   --no-source \
                   -s main,restricted,universe,multiverse \
                   -h fr.archive.ubuntu.com \
                   -d ${DISTRO},${DISTRO}-security,${DISTRO}-updates,${DISTRO}-backports \
                   -r /ubuntu \
                   --no-check-gpg \
                   --ignore-release-gpg \
                   --progress \
                   --method=http \
                   --i18n \
                   --getcontents \
                   ${cible}
