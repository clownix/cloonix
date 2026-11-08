#!/bin/bash
# Mirror Arch Linux core and extra x86_64 and the bootstrap tarball into the
# arch tree, updated in place.
. "$(dirname "$0")/distro_common.sh"
UP=rsync://frankfurt.mirror.pkgbuild.com/packages
# exFAT refuses ':': an epoch package is fetched over http under a name with
# .epoch. for its ':', and the .db made to name it so.
HTTP_BASES="https://frankfurt.mirror.pkgbuild.com https://geo.mirror.pkgbuild.com"
TOKEN=.epoch.
EPOCH_JOBS=${EPOCH_JOBS:-8}

# fix_epoch <repo> <dir holding its .db>
fix_epoch() {
  local repo=$1 dst=$2 db=$2/$1.db work inv todo total want desc fn safe
  local count=0 nleft=0 kept f b
  work=$(mktemp -d)
  # A .db is a compressed tar whatever its name.
  if ! tar -xf "${db}" -C "${work}"; then
    echo "ERROR: ${db} cannot be read"
    rm -rf "${work}"
    return 1
  fi
  inv=$(mktemp)
  find "${work}" -name desc -exec awk '
      /^%FILENAME%$/ { getline; if (index($0, ":")) print FILENAME "\t" $0 }
    ' {} + > "${inv}"
  total=$(wc -l < "${inv}")
  if [ "${total}" = 0 ]; then
    rm -rf "${inv}" "${work}"
    return 0
  fi
  todo=$(mktemp)
  while IFS=$'\t' read -r desc fn; do
    [ -s "${dst}/${fn//:/${TOKEN}}" ] || echo "${fn}" >> "${todo}"
  done < "${inv}"
  want=$(wc -l < "${todo}")
  if [ "${want}" != 0 ]; then
    echo "epoch: ${repo} has ${total} epoch packages, ${want} to fetch with ${EPOCH_JOBS} jobs"
    EPOCH_DST=${dst} EPOCH_REPO=${repo} EPOCH_TOK=${TOKEN} EPOCH_BASES=${HTTP_BASES} \
    xargs -a "${todo}" -r -n1 -P "${EPOCH_JOBS}" sh -c '
      out="${EPOCH_DST}/$(printf "%s" "$1" | sed "s|:|${EPOCH_TOK}|g")"
      for base in ${EPOCH_BASES}; do
        url="${base}/${EPOCH_REPO}/os/x86_64/$1"
        # .part then rename: a killed wget leaves no truncated package under
        # the name a later run takes as complete.
        if wget -q -O "${out}.part" "${url}"; then
          mv -f "${out}.part" "${out}"
          wget -q -O "${out}.sig.part" "${url}.sig" && mv -f "${out}.sig.part" "${out}.sig"
          rm -f "${out}.sig.part"
          exit 0
        fi
        rm -f "${out}.part"
      done
      echo "ERROR: epoch package $1 not fetched from ${EPOCH_BASES}"
      exit 1
    ' _
  fi
  while IFS=$'\t' read -r desc fn; do
    safe=${fn//:/${TOKEN}}
    if [ -s "${dst}/${safe}" ]; then
      sed -i "/^%FILENAME%\$/{n;s|${fn}|${safe}|}" "${desc}"
      count=$((count + 1))
    else
      nleft=$((nleft + 1))
    fi
  done < "${inv}"
  rm -f "${inv}" "${todo}"
  if [ "${count}" -gt 0 ]; then
    (cd "${work}" && tar -czf "${db}.tmp" -- */) && mv -f "${db}.tmp" "${db}"
    echo "epoch: ${repo}.db names ${count} of ${total} epoch packages without ':'"
  fi
  [ "${nleft}" = 0 ] || echo "ERROR: ${nleft} epoch packages of ${repo} missing, a build needing one fails on a 404"
  # An epoch package the .db no longer names is an old version.
  kept=$(mktemp)
  find "${work}" -name desc -exec awk '/^%FILENAME%$/ { getline; print }' {} + > "${kept}"
  for f in "${dst}"/*"${TOKEN}"*.pkg.tar.zst; do
    [ -e "${f}" ] || continue
    b=${f##*/}
    grep -qxF "${b}" "${kept}" || { echo "epoch: prune ${b}"; rm -f "${f}" "${f}.sig"; }
  done
  rm -rf "${kept}" "${work}"
  [ "${nleft}" = 0 ]
}

for repo in core extra; do
  # Neither fetched nor deleted by rsync: what has a ':' and its renamed copy.
  mirror_rsync "${UP}/${repo}/os/x86_64/" "arch/${repo}/os/x86_64" \
               --exclude='*:*' --exclude="*${TOKEN}*"
  if need "arch/${repo}/os/x86_64/${repo}.db"; then
    fix_epoch "${repo}" "${STORE}/arch/${repo}/os/x86_64" || RC=1
  fi
done
ISO=${STORE}/arch/iso/latest
if ! { mkdir -p "${ISO}" && wget -nv -N -P "${ISO}" \
       https://geo.mirror.pkgbuild.com/iso/latest/archlinux-bootstrap-x86_64.tar.zst; }; then
  echo "ERROR: the bootstrap tarball is not fetched"
  RC=1
fi
need arch/iso/latest/archlinux-bootstrap-x86_64.tar.zst
finish arch
