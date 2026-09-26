#!/bin/bash
# Mirror Alpine Linux (main + community, x86_64) to local disk with rsync.
set -u

# Configuration ----------------------------------------------------#
ALPINE_VER="v3.24"
ARCH="x86_64"
RSYNC_HOST="rsync.alpinelinux.org"
# Destination root.
DEST="/media/perrier/T7Shield2/distros/alpine"
# Bandwidth cap in KiB/s (0 = unlimited).
BWLIMIT_KBPS=0

if [ -z "${DEST}" ]; then
  echo "FATAL: DEST is empty; refusing to run."
  exit 1
fi
case "${DEST}" in
  /|/root|/home|/etc|/usr|/var|/bin|/lib*|/boot|/dev|/proc|/sys|/tmp)
    echo "FATAL: DEST='${DEST}' is a top-level system path; refusing to run."
    exit 1 ;;
esac
case "${DEST}" in
  /*/*) : ;;
  *) echo "FATAL: DEST='${DEST}' must be an absolute path with >=2 components."; exit 1 ;;
esac

if ! command -v rsync >/dev/null 2>&1; then
  echo "FATAL: rsync is not installed.  sudo apt install rsync"
  exit 1
fi

# rsync: -a archive, --delete-after so the tree stays usable during the sync.
RSYNC_OPTS=(-av --delete --delete-after --human-readable
            --contimeout=30 --timeout=600)
[ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && RSYNC_OPTS+=("--bwlimit=${BWLIMIT_KBPS}")

# The two repositories to mirror.
REPOS=(main community)

overall_rc=0
for repo in "${REPOS[@]}"; do
  SRC="rsync://${RSYNC_HOST}/alpine/${ALPINE_VER}/${repo}/${ARCH}/"
  DST="${DEST}/${repo}/${ARCH}/"
  mkdir -p "${DST}"
  echo "=============================================================="
  echo " Alpine mirror : ${ALPINE_VER} ${repo}/${ARCH}"
  echo "   source : ${SRC}"
  echo "   dest   : ${DST}"
  [ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && echo "   bwlimit: ${BWLIMIT_KBPS} KiB/s"
  echo "=============================================================="

  rsync "${RSYNC_OPTS[@]}" "${SRC}" "${DST}"
  rc=$?

  if [ "${rc}" != "0" ]; then
    echo "WARNING: rsync for ${repo} exited with code ${rc}."
    overall_rc=${rc}
  fi

  if [ ! -e "${DST}/APKINDEX.tar.gz" ]; then
    echo "WARNING: ${DST}APKINDEX.tar.gz not present -- check that"
    echo "         ALPINE_VER='${ALPINE_VER}' and repo='${repo}' are correct."
    [ "${overall_rc}" = "0" ] && overall_rc=1
  fi
  echo
done

REL_DIR="${DEST}/releases/${ARCH}"
HTTP_BASE="https://dl-cdn.alpinelinux.org/alpine"
mkdir -p "${REL_DIR}"
echo "=============================================================="
echo " Alpine minirootfs tarball"
echo "   source : ${HTTP_BASE}/${ALPINE_VER}/releases/${ARCH}/"
echo "   dest   : ${REL_DIR}/"
echo "=============================================================="
LISTING=$(wget -q -O- "${HTTP_BASE}/${ALPINE_VER}/releases/${ARCH}/" 2>/dev/null)
MINIROOT=$(echo "${LISTING}" \
           | grep -o "alpine-minirootfs-[0-9][0-9.]*-${ARCH}\.tar\.gz" \
           | sort -V | tail -1)
if [ -n "${MINIROOT}" ]; then
  if wget -N -P "${REL_DIR}" \
          "${HTTP_BASE}/${ALPINE_VER}/releases/${ARCH}/${MINIROOT}"; then
    echo "minirootfs present: ${MINIROOT} ($(du -h "${REL_DIR}/${MINIROOT}" | cut -f1))"
    # A stable name so kvm_alpine.sh does not need editing on every point
    # release.
    ln -sf "${MINIROOT}" "${REL_DIR}/alpine-minirootfs-latest-${ARCH}.tar.gz" 2>/dev/null || \
      cp -f "${REL_DIR}/${MINIROOT}" "${REL_DIR}/alpine-minirootfs-latest-${ARCH}.tar.gz"
    echo "stable name: alpine-minirootfs-latest-${ARCH}.tar.gz -> ${MINIROOT}"
  else
    echo "WARNING: could not fetch ${MINIROOT}"
    [ "${overall_rc}" = "0" ] && overall_rc=1
  fi
else
  echo "WARNING: no alpine-minirootfs-*.tar.gz found in the listing for"
  echo "         ${ALPINE_VER}/releases/${ARCH}; kvm_alpine.sh will have"
  echo "         no bootstrap tarball."
  [ "${overall_rc}" = "0" ] && overall_rc=1
fi
echo
if [ "${overall_rc}" = "0" ]; then
  echo "DONE: Alpine ${ALPINE_VER} (${REPOS[*]}) + minirootfs mirrored to ${DEST}"
else
  echo "COMPLETED WITH WARNINGS (rc=${overall_rc}); see messages above."
fi
exit ${overall_rc}
