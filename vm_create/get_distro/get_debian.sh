#!/bin/bash
# Mirror Debian amd64, a tree per release named by its codename, holding the
# release and its -updates; its -security, served by another host, in a tree
# of its own: <codename>-security.
# ./get_debian.sh          every release of RELEASES
# ./get_debian.sh trixie   only the releases named
RELEASES=(trixie)
[ $# -gt 0 ] && RELEASES=("$@")
. "$(dirname "$0")/distro_common.sh"
SECTIONS=main,contrib,non-free,non-free-firmware
for r in "${RELEASES[@]}"; do
  mirror_deb ftp.fr.debian.org debian "${SECTIONS}" "${r}" "${r},${r}-updates"
  mirror_deb security.debian.org debian-security "${SECTIONS}" "${r}-security" "${r}-security"
done
finish "debian ${RELEASES[*]}"
