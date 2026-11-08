#!/bin/bash
# Sourced by the scripts of get_distro: the store, the two ways a tree is
# mirrored, the check of a file a builder reads and the last line of a run.
set -u
STORE=/media/perrier/T7Shield2/distros
RC=0
# The store is exFAT: no symlink (-L copies the target), no owner, no mode.
# New files are renamed into place together at the end of each transfer.
RSYNC_OPTS=(-rtvL --delete-delay --delay-updates --contimeout=30 --timeout=600)

# mirror_rsync <source dir/> <dir in STORE> [rsync option ...]
mirror_rsync() {
  local src=$1 dst=${STORE}/$2/
  shift 2
  echo "=== ${src} -> ${dst}"
  mkdir -p "${dst}" || { RC=1; return; }
  rsync "${RSYNC_OPTS[@]}" "$@" "${src}" "${dst}" || { echo "ERROR: rsync ${src} rc=$?"; RC=1; }
}

# mirror_deb <host> <root> <sections> <tree> <dists> [debmirror option ...]
# The dists into one tree: amd64, no source, every Translation, the Contents.
# exFAT: no dists/stable -> trixie symlink. No rsync for trace files.
mirror_deb() {
  local host=$1 root=$2 sections=$3 tree=$4 dists=$5 d
  shift 5
  echo "=== http://${host}/${root} ${dists} -> ${STORE}/${tree}"
  debmirror --arch=amd64 --no-source --i18n --getcontents --omit-suite-symlinks \
            --rsync-extra=none --method=http --host="${host}" --root="${root}" \
            --dist="${dists}" --section="${sections}" --no-check-gpg --progress \
            "$@" "${STORE}/${tree}" || { echo "ERROR: debmirror ${tree} rc=$?"; RC=1; }
  for d in ${dists//,/ }; do need "${tree}/dists/${d}/Release"; done
}

# need <file in STORE>: a file a builder reads.
need() {
  [ -e "${STORE}/$1" ] && return 0
  echo "ERROR: ${STORE}/$1 missing"
  RC=1
  return 1
}

# finish <what>: the last line and the exit code of the run.
finish() {
  if [ "${RC}" = 0 ]; then echo "DONE: $*"; else echo "COMPLETED WITH ERRORS: $*"; fi
  exit "${RC}"
}
