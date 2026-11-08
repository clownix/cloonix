#!/bin/bash
# Mirror openSUSE Tumbleweed oss into the tumbleweed tree, updated in place.
. "$(dirname "$0")/distro_common.sh"
mirror_rsync rsync://rsync.opensuse.org/opensuse-full-with-factory/opensuse/tumbleweed/repo/oss/ \
             tumbleweed/repo/oss
need tumbleweed/repo/oss/repodata/repomd.xml
finish tumbleweed
