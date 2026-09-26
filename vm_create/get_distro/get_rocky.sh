#!/bin/bash
# Mirror Rocky Linux (BaseOS + AppStream, x86_64) to local disk with rsync.
set -u
ROCKY_VER="10"
ARCH="x86_64"
# msync.rockylinux.org is for official mirror PARTNERS only (it greets end
# users and refuses the version path).
RSYNC_BASE="rsync://mirror.rackspace.com/rocky"
DEST="/media/perrier/T7Shield2/distros/rocky"
BWLIMIT_KBPS=0
REPOS=(BaseOS AppStream)
if [ -z "${DEST}" ]; then echo "FATAL: DEST empty"; exit 1; fi
case "${DEST}" in
  /|/root|/home|/etc|/usr|/var|/bin|/lib*|/boot|/dev|/proc|/sys|/tmp)
    echo "FATAL: DEST='${DEST}' is a top-level system path"; exit 1 ;;
esac
case "${DEST}" in /*/*) : ;; *) echo "FATAL: DEST must be absolute >=2 comps"; exit 1 ;; esac
command -v rsync >/dev/null 2>&1 || { echo "FATAL: rsync not installed (sudo apt install rsync)"; exit 1; }
RSYNC_OPTS=(-av --delete --delete-after --human-readable --contimeout=30 --timeout=600)
[ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && RSYNC_OPTS+=("--bwlimit=${BWLIMIT_KBPS}")

overall_rc=0
for repo in "${REPOS[@]}"; do
  SRC="${RSYNC_BASE}/${ROCKY_VER}/${repo}/${ARCH}/os/"
  DST="${DEST}/${ROCKY_VER}/${repo}/${ARCH}/os/"
  mkdir -p "${DST}"
  echo "=============================================================="
  echo " Rocky mirror : ${ROCKY_VER} ${repo}/${ARCH}"
  echo "   source : ${SRC}"
  echo "   dest   : ${DST}"
  echo "=============================================================="
  rsync "${RSYNC_OPTS[@]}" "${SRC}" "${DST}"
  rc=$?
  [ "${rc}" != "0" ] && { echo "WARNING: rsync ${repo} rc=${rc}"; overall_rc=${rc}; }
  if [ ! -e "${DST}/repodata/repomd.xml" ]; then
    echo "WARNING: ${DST}repodata/repomd.xml missing -- check ROCKY_VER/repo path."
    [ "${overall_rc}" = "0" ] && overall_rc=1
  fi
  echo
done
[ "${overall_rc}" = "0" ] && echo "DONE: Rocky ${ROCKY_VER} mirrored to ${DEST}" \
                          || echo "COMPLETED WITH WARNINGS (rc=${overall_rc})."
exit ${overall_rc}
