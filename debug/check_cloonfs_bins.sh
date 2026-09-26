#!/bin/bash
# Does the rootfs tarball carry every binary cloonix itself runs?
set -e
HERE=$(cd $(dirname $0) && pwd)
PTHEXEC=${HERE}/../cloonix/cloonsrc/glob_include/pthexec.c
TARBALL=${1:-}

if [ -z "${TARBALL}" ]; then
  TARBALL=$(ls ${HOME}/cloonix_data/targz_store/*base_rootfs*.tar.gz 2>/dev/null | head -1)
fi
if [ ! -f "${TARBALL}" ]; then
  echo "usage: $0 <base_rootfs.tar.gz>"
  exit 1
fi

wanted=$(grep -oE '"/cloonfs/(bin|sbin)/[a-z0-9_.-]+"' ${PTHEXEC} \
         | tr -d '"' | sed 's|/cloonfs/||' \
         | grep -v '/cloonix-' | sort -u)

have=$(tar tzf ${TARBALL} | sed 's|^\./||' | grep -E '^(bin|sbin)/' | sort -u)

missing=0
for w in ${wanted}; do
  if ! echo "${have}" | grep -qx "${w}"; then
    echo "MISSING from the rootfs: ${w}"
    grep -n "cloonfs/${w}\"" ${PTHEXEC} | head -1 | sed 's/^/    wanted by pthexec.c:/'
    missing=1
  fi
done

if [ ${missing} -eq 0 ]; then
  echo "the rootfs carries every binary pthexec names ($(echo "${wanted}" | wc -w) checked)"
else
  echo
  echo "cloonix runs these itself, on the host, out of cloonfs. A switch in"
  echo "rootfs_targz_store.sh that drops one makes cloonix fail at run time,"
  echo "not at build time."
  exit 1
fi
