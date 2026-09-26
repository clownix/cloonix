#!/bin/bash
# Mirror Arch Linux (core + extra, x86_64) to local disk with rsync.
set -u
ARCH="x86_64"
RSYNC_BASE="rsync://frankfurt.mirror.pkgbuild.com/packages"
# HTTP mirror for the epoch packages rsync cannot write (see EPOCH note
# below).
RSYNC_HOST="${RSYNC_BASE#rsync://}"
RSYNC_HOST="${RSYNC_HOST%%/*}"
HTTP_PKG_BASES="https://${RSYNC_HOST} https://geo.mirror.pkgbuild.com"
DEST="/media/perrier/T7Shield2/distros/arch"
BWLIMIT_KBPS=0
REPOS=(core extra)
EPOCH_TOKEN=".epoch."
if [ -z "${DEST}" ]; then echo "FATAL: DEST empty"; exit 1; fi
case "${DEST}" in
  /|/root|/home|/etc|/usr|/var|/bin|/lib*|/boot|/dev|/proc|/sys|/tmp)
    echo "FATAL: DEST='${DEST}' is a top-level system path"; exit 1 ;;
esac
case "${DEST}" in /*/*) : ;; *) echo "FATAL: DEST must be absolute >=2 comps"; exit 1 ;; esac
command -v rsync >/dev/null 2>&1 || { echo "FATAL: rsync not installed (sudo apt install rsync)"; exit 1; }
RSYNC_OPTS=(-avL --delete --delete-after --delay-updates
            --exclude '*:*' --exclude '*.epoch.*'
            --human-readable --contimeout=30 --timeout=600)
[ "${BWLIMIT_KBPS}" -gt 0 ] 2>/dev/null && RSYNC_OPTS+=("--bwlimit=${BWLIMIT_KBPS}")
# fct_fix_epoch <repo> <dstdir>: exFAT (the mirror SSD) cannot hold ':' in epoch package names.
EPOCH_JOBS="${EPOCH_JOBS:-8}"

fct_fix_epoch() {
  local repo="$1" dst="$2"
  local db="${dst}/${repo}.db"
  [ -e "${db}" ] || { echo "  epoch: no ${repo}.db, skipping"; return 0; }
  local work; work=$(mktemp -d)
  # .db is a gzipped tar even though the name has no .gz
  if ! tar xzf "${db}" -C "${work}" 2>/dev/null; then
    echo "  epoch: ${repo}.db is not a gzip tar, skipping"; rm -rf "${work}"; return 0
  fi

  local inv; inv=$(mktemp)
  find "${work}" -name desc -exec awk '
      /^%FILENAME%$/ { getline; if (index($0, ":")) print FILENAME "\t" $0 }
    ' {} + > "${inv}" 2>/dev/null

  local total; total=$(wc -l < "${inv}")
  if [ "${total}" = "0" ]; then
    echo "  epoch: no epoch packages in ${repo}"
    rm -f "${inv}"; rm -rf "${work}"
    return 0
  fi

  # Pass 1: fetch what is not on the drive yet.
  local todo; todo=$(mktemp)
  local desc fn safe
  while IFS="$(printf '\t')" read -r desc fn; do
    safe=${fn//:/${EPOCH_TOKEN}}
    [ -s "${dst}/${safe}" ] || echo "${fn}" >> "${todo}"
  done < "${inv}"

  local want; want=$(wc -l < "${todo}")
  if [ "${want}" != "0" ]; then
    echo "  epoch: ${repo} has ${total} epoch package(s), ${want} not on the drive"
    echo "         fetching with ${EPOCH_JOBS} parallel jobs, this takes a while"
    EPOCH_DST="${dst}" EPOCH_REPO="${repo}" EPOCH_ARCH="${ARCH}" \
    EPOCH_TOK="${EPOCH_TOKEN}" EPOCH_BASES="${HTTP_PKG_BASES}" \
    xargs -a "${todo}" -r -n1 -P "${EPOCH_JOBS}" sh -c '
      fn="$1"
      safe=$(printf "%s" "${fn}" | sed "s|:|${EPOCH_TOK}|g")
      out="${EPOCH_DST}/${safe}"
      [ -s "${out}" ] && exit 0
      for base in ${EPOCH_BASES}; do
        url="${base}/${EPOCH_REPO}/os/${EPOCH_ARCH}/${fn}"
        # .part then rename: a killed wget must not leave a truncated package
        # sitting under the name later runs take as complete.
        if wget -q -O "${out}.part" "${url}"; then
          mv -f "${out}.part" "${out}"
          if wget -q -O "${out}.sig.part" "${url}.sig" 2>/dev/null; then
            mv -f "${out}.sig.part" "${out}.sig"
          else
            rm -f "${out}.sig.part"
          fi
          exit 0
        fi
        rm -f "${out}.part"
      done
      echo "  epoch: FAILED ${fn}"
      exit 1
    ' _
  fi

  local count=0 fail=0 left=""
  while IFS="$(printf '\t')" read -r desc fn; do
    safe=${fn//:/${EPOCH_TOKEN}}
    if [ -s "${dst}/${safe}" ]; then
      sed -i "/^%FILENAME%\$/{n;s|${fn}|${safe}|}" "${desc}"
      count=$((count+1))
    else
      left="${left}${fn}
"
      fail=1
    fi
  done < "${inv}"
  rm -f "${inv}" "${todo}"

  if [ "${count}" -gt 0 ]; then
    ( cd "${work}" && tar czf "${db}.tmp" -- */ ) && mv -f "${db}.tmp" "${db}"
    local fdb="${dst}/${repo}.files"
    if [ -e "${fdb}" ]; then
      local fwork; fwork=$(mktemp -d)
      if tar xzf "${fdb}" -C "${fwork}" 2>/dev/null; then
        find "${fwork}" -name desc -exec \
          sed -i "/^%FILENAME%\$/{n;s|:|${EPOCH_TOKEN}|g}" {} \;
        ( cd "${fwork}" && tar czf "${fdb}.tmp" -- */ ) && mv -f "${fdb}.tmp" "${fdb}"
      fi
      rm -rf "${fwork}"
    fi
    echo "  epoch: ${repo}.db now points at ${count} of ${total} colon-free name(s)"
  fi

  if [ -n "${left}" ]; then
    local nleft; nleft=$(printf '%s' "${left}" | grep -c .)
    echo "  epoch: WARNING ${nleft} package(s) of ${repo} are still named with"
    echo "         a ':' and are NOT on the drive:"
    printf '%s' "${left}" | head -10 | sed 's/^/           /'
    [ "${nleft}" -gt 10 ] && echo "           ... and $((nleft-10)) more"
    echo "         A build that needs one of them will fail on a 404. Run"
    echo "         this script again: what was fetched is kept, so each run"
    echo "         gets closer."
  fi

  local kept; kept=$(mktemp)
  find "${work}" -name desc -exec awk '
      /^%FILENAME%$/ { getline; print }
    ' {} + > "${kept}" 2>/dev/null
  local f b
  for f in "${dst}"/*"${EPOCH_TOKEN}"*.pkg.tar.zst; do
    [ -e "${f}" ] || continue
    b=$(basename "${f}")
    grep -qxF "${b}" "${kept}" || { echo "  epoch: prune stale ${b}"; rm -f "${f}" "${f}.sig"; }
  done
  rm -f "${kept}"
  rm -rf "${work}"
  return ${fail}
}
# Prove, before transferring anything, that the colon handling is real.
echo "=============================================================="
echo " get_arch.sh : $(readlink -f "$0")"
echo "   dest      : ${DEST}"
mkdir -p "${DEST}" 2>/dev/null
FSTYPE=$(df -PT "${DEST}" 2>/dev/null | awk 'NR==2{print $2}')
echo "   filesystem: ${FSTYPE:-unknown}"
COLON_PROBE="${DEST}/.colon:probe"
if : > "${COLON_PROBE}" 2>/dev/null; then
  rm -f "${COLON_PROBE}"
  COLON_OK=1
  echo "   colons    : the destination accepts ':' in a filename"
  echo "               (the epoch workaround is not needed here, but is"
  echo "                harmless and stays on)"
else
  COLON_OK=0
  echo "   colons    : the destination REFUSES ':' in a filename, which is"
  echo "               normal for exFAT and NTFS. Epoch packages are"
  echo "               excluded from rsync and fetched separately under a"
  echo "               colon-free name (: -> ${EPOCH_TOKEN})."
fi
case " ${RSYNC_OPTS[*]} " in
  *" *:* "*) echo "   exclude   : '*:*' is active" ;;
  *) echo "   exclude   : FATAL '*:*' is NOT in the rsync arguments."
     echo "               rsync will try to write the epoch packages and"
     echo "               fail on every one of them. Fix RSYNC_OPTS."
     exit 1 ;;
esac
echo "=============================================================="
echo
overall_rc=0
for repo in "${REPOS[@]}"; do
  SRC="${RSYNC_BASE}/${repo}/os/${ARCH}/"
  DST="${DEST}/${repo}/os/${ARCH}/"
  mkdir -p "${DST}"
  echo "=============================================================="
  echo " Arch mirror : ${repo}/${ARCH}"
  echo "   source : ${SRC}"
  echo "   dest   : ${DST}"
  echo "=============================================================="
  rsync "${RSYNC_OPTS[@]}" "${SRC}" "${DST}"
  rc=$?
  [ "${rc}" != "0" ] && { echo "WARNING: rsync ${repo} rc=${rc}"; overall_rc=${rc}; }
  # Arch repo DB is <repo>.db (a symlink to <repo>.db.tar.gz).
  if [ ! -e "${DST}/${repo}.db" ]; then
    echo "WARNING: ${DST}${repo}.db missing -- check repo path/mirror."
    [ "${overall_rc}" = "0" ] && overall_rc=1
  fi
  fct_fix_epoch "${repo}" "${DST%/}" || \
    { echo "WARNING: some epoch packages for ${repo} failed"; \
      [ "${overall_rc}" = "0" ] && overall_rc=1; }
  echo
done
ISO_DIR="${DEST}/iso/latest"
BOOTSTRAP="archlinux-bootstrap-${ARCH}.tar.zst"
HTTP_BASE="https://geo.mirror.pkgbuild.com"
mkdir -p "${ISO_DIR}"
echo "=============================================================="
echo " Arch bootstrap tarball"
echo "   source : ${HTTP_BASE}/iso/latest/${BOOTSTRAP}"
echo "   dest   : ${ISO_DIR}/${BOOTSTRAP}"
echo "=============================================================="
# N keeps the local copy when it is not older than the remote one, so re-
# running this script does not re-download 120M every time.
if wget -N -P "${ISO_DIR}" "${HTTP_BASE}/iso/latest/${BOOTSTRAP}"; then
  # The sha256 file lets a build verify the tarball if it wants to.
  wget -N -P "${ISO_DIR}" "${HTTP_BASE}/iso/latest/sha256sums.txt" 2>/dev/null || true
  echo "bootstrap tarball present: $(du -h "${ISO_DIR}/${BOOTSTRAP}" | cut -f1)"
else
  echo "WARNING: could not fetch ${BOOTSTRAP}"
  echo "         kvm_arch.sh will fall back to the internet for it."
  [ "${overall_rc}" = "0" ] && overall_rc=1
fi
echo
[ "${overall_rc}" = "0" ] && echo "DONE: Arch (${REPOS[*]}) + bootstrap mirrored to ${DEST}" \
                          || echo "COMPLETED WITH WARNINGS (rc=${overall_rc})."
exit ${overall_rc}
