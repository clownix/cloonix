#!/bin/bash
# Mirror Alpine main and community x86_64 and the newest minirootfs into the
# alpine tree, updated in place: a new release replaces the old one.
. "$(dirname "$0")/distro_common.sh"
VER=v3.24
for repo in main community; do
  mirror_rsync "rsync://rsync.alpinelinux.org/alpine/${VER}/${repo}/x86_64/" "alpine/${repo}/x86_64"
  need "alpine/${repo}/x86_64/APKINDEX.tar.gz"
done
# The newest minirootfs, also under the name the builders read.
URL=https://dl-cdn.alpinelinux.org/alpine/${VER}/releases/x86_64
DIR=${STORE}/alpine/releases/x86_64
MINI=$(wget -nv -O- "${URL}/" | grep -o 'alpine-minirootfs-[0-9][0-9.]*-x86_64\.tar\.gz' | sort -uV | tail -1)
if [ -n "${MINI}" ] && mkdir -p "${DIR}" && wget -nv -N -P "${DIR}" "${URL}/${MINI}"; then
  cp --remove-destination "${DIR}/${MINI}" "${DIR}/alpine-minirootfs-latest-x86_64.tar.gz"
  find "${DIR}" -name 'alpine-minirootfs-[0-9]*' ! -name "${MINI}" -delete
else
  echo "ERROR: no minirootfs fetched from ${URL}/"
  RC=1
fi
need alpine/releases/x86_64/alpine-minirootfs-latest-x86_64.tar.gz
finish "alpine ${VER}"
