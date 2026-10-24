#!/bin/bash
# Mirror the openSUSE Tumbleweed OSS repository to local disk with rsync.
set -u

# Configuration ----------------------------------------------------#
RSYNC_HOST="rsync.opensuse.org"
MODULE="opensuse-full-with-factory"
# Destination: where the mirror tree is written.
DEST="/media/perrier/T7Shield2/distros"
# Bandwidth cap in KiB/s (0 = unlimited).
BWLIMIT_KBPS=0
INCLUDES=(
  "--include=opensuse/"
  "--include=opensuse/tumbleweed/"
  "--include=opensuse/tumbleweed/repo/"
  "--include=opensuse/tumbleweed/repo/oss/***"
  "--exclude=*"
)

if [ -z "${DEST}" ]; then
  echo "FATAL: DEST is empty; refusing to run."
  exit 1
fi
case "${DEST}" in
  /|/root|/home|/etc|/usr|/var|/bin|/lib*|/boot|/dev|/proc|/sys|/tmp)
    echo "FATAL: DEST='${DEST}' is a top-level system path; refusing to run."
    exit 1 ;;
esac
# Require an absolute path with at least two path components (e.g.
case "${DEST}" in
  /*/*) : ;;
  *) echo "FATAL: DEST='${DEST}' must be an absolute path with >=2 components."; exit 1 ;;
esac

if ! command -v rsync >/dev/null 2>&1; then
  echo "FATAL: rsync is not installed.  sudo apt install rsync"
  exit 1
fi

mkdir -p "${DEST}"
if [ ! -d "${DEST}" ]; then
  echo "FATAL: could not create DEST='${DEST}'."
  exit 1
fi

COMMON_OPTS=(-rltH -p --info=progress2 --human-readable)
[ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && COMMON_OPTS+=("--bwlimit=${BWLIMIT_KBPS}")

SRC="rsync://${RSYNC_HOST}/${MODULE}/"

OSS_LOCAL="${DEST}/opensuse/tumbleweed/repo/oss"
if [ -d "${OSS_LOCAL}" ] && [ -n "$(ls -A "${OSS_LOCAL}" 2>/dev/null)" ]; then
  MODE="maintenance"
  MODE_OPTS=(--delete-delay --delay-updates)
else
  MODE="initial"
  # First fill: no --delete (tree incomplete), no --delay-updates (does not resume).
  MODE_OPTS=(--ignore-existing)
fi

echo "=============================================================="
echo " openSUSE Tumbleweed mirror"
echo "   source : ${SRC}"
echo "   dest   : ${DEST}"
echo "   mode   : ${MODE}"
echo "   filter : tumbleweed/repo/oss (x86_64 + noarch inside)"
[ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && echo "   bwlimit: ${BWLIMIT_KBPS} KiB/s"
echo "=============================================================="
echo

# VERIFY_LAYOUT=1: list the module's top level before syncing.
if [ "${VERIFY_LAYOUT:-0}" = "1" ]; then
  echo "--- module top level (rsync ${SRC}) ---"
  rsync --list-only "${SRC}" 2>/dev/null | head -30
  echo "--- tumbleweed/repo under module ---"
  rsync --list-only "${SRC}opensuse/tumbleweed/repo/" 2>/dev/null | head -30
  echo "----------------------------------------"
  echo "If the paths above don't include opensuse/tumbleweed/repo/oss,"
  echo "adjust the INCLUDES array to match, then run without VERIFY_LAYOUT."
  exit 0
fi

# --contimeout/--timeout: a stalled connection fails instead of hanging.
rsync "${COMMON_OPTS[@]}" "${MODE_OPTS[@]}" \
      --contimeout=30 --timeout=600 --stats \
      "${INCLUDES[@]}" \
      "${SRC}" "${DEST}/"
RC=$?

if [ "${RC}" = "0" ] && [ ! -e "${DEST}/opensuse/tumbleweed/repo/oss/repodata/repomd.xml" ]; then
  echo
  echo "WARNING: sync reported success but no repo metadata is present at"
  echo "  ${DEST}/opensuse/tumbleweed/repo/oss/repodata/repomd.xml"
  echo "This almost always means the rsync include paths don't match the"
  echo "server's module layout. Re-run with VERIFY_LAYOUT=1 to inspect it:"
  echo "VERIFY_LAYOUT=1 ${0}"
fi

echo
if [ "${RC}" = "0" ]; then
  echo "DONE (${MODE} pass complete)."
  if [ "${MODE}" = "initial" ]; then
    echo "Tip: re-run this script to perform an incremental maintenance pass"
    echo "     (it will switch to consistent --delete-delay mode automatically)."
  fi
elif [ "${RC}" = "23" ] || [ "${RC}" = "24" ]; then
  echo "PARTIAL (rsync rc=${RC}) -- some files changed/vanished mid-sync."
  echo "Re-run the script to finish; this is normal for a live repository."
else
  echo "ERROR: rsync exited with code ${RC}."
  echo "If it says 'connection limit', the public server was busy (max 50);"
  echo "wait and retry, or use a regional mirror from"
  echo "https://mirrors.opensuse.org that offers rsync."
fi
exit ${RC}
