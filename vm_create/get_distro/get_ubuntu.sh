#!/bin/bash
# Mirror Ubuntu amd64, a tree per release named by its codename, holding the
# release, its -security and its -updates.
# ./get_ubuntu.sh            every release of RELEASES
# ./get_ubuntu.sh resolute   only the releases named
RELEASES=(resolute)
[ $# -gt 0 ] && RELEASES=("$@")
. "$(dirname "$0")/distro_common.sh"
for r in "${RELEASES[@]}"; do
  mirror_deb fr.archive.ubuntu.com ubuntu main,universe,multiverse "${r}" \
             "${r},${r}-security,${r}-updates" --exclude='google-android.*'
done
finish "ubuntu ${RELEASES[*]}"
