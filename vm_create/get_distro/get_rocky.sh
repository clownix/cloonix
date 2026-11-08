#!/bin/bash
# Mirror Rocky Linux BaseOS and AppStream x86_64 into the rocky tree, a
# release per major number laid out as upstream: <N>, which follows the
# newest minor release of N.
# ./get_rocky.sh       every release of RELEASES
# ./get_rocky.sh 10    only the releases named
RELEASES=(10)
[ $# -gt 0 ] && RELEASES=("$@")
. "$(dirname "$0")/distro_common.sh"
# msync.rockylinux.org refuses whoever is not an official mirror partner.
UP=rsync://mirror.rackspace.com/rocky
for n in "${RELEASES[@]}"; do
  for repo in BaseOS AppStream; do
    mirror_rsync "${UP}/${n}/${repo}/x86_64/os/" "rocky/${n}/${repo}/x86_64/os"
    need "rocky/${n}/${repo}/x86_64/os/repodata/repomd.xml"
  done
done
finish "rocky ${RELEASES[*]}"
