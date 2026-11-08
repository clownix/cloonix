#!/bin/bash
# Mirror Fedora Everything x86_64 into the fedora tree, a release per number
# laid out as upstream: releases/<N>, updates/<N>.
# ./get_fedora.sh       every release of RELEASES
# ./get_fedora.sh 45    only the releases named
# A release gone upstream fails here and stays on disk.
RELEASES=(44 45)
[ $# -gt 0 ] && RELEASES=("$@")
. "$(dirname "$0")/distro_common.sh"
UP=rsync://ftp.fau.de/fedora/linux
for n in "${RELEASES[@]}"; do
  # os/ only: its siblings debug/ and iso/ are not wanted.
  mirror_rsync "${UP}/releases/${n}/Everything/x86_64/os/" "fedora/releases/${n}/Everything/x86_64/os"
  # debug/ excluded, and an excluded path is never deleted here.
  mirror_rsync "${UP}/updates/${n}/Everything/x86_64/" "fedora/updates/${n}/Everything/x86_64" --exclude=debug/
  need "fedora/releases/${n}/Everything/x86_64/os/repodata/repomd.xml"
  need "fedora/updates/${n}/Everything/x86_64/repodata/repomd.xml"
done
finish "fedora ${RELEASES[*]}"
