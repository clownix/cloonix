#!/bin/bash
# Asterisk and the pjproject it bundles into the asterisk tree, under the
# names cnt_kvm_demo/asterisk_build.sh reads, pinned by their sha256: the tree
# holds those files or nothing. Debian trixie carries no asterisk (bug
# 1031046): asterisk_trixie compiles it from these. 22 LTS, supported until
# October 2029; the pjproject version is the one of its third-party/versions.mak.
. "$(dirname "$0")/distro_common.sh"
V=22.12.0
P=2.17
FILES=(
  "https://github.com/asterisk/asterisk/releases/download/${V}/asterisk-${V}.tar.gz asterisk.tar.gz f8b2ba98aac6c4ff3376bc0a2868bea6ed577ca684efb7ec739ae6e8ce6e35e2"
  "https://raw.githubusercontent.com/asterisk/third-party/master/pjproject/${P}/pjproject-${P}.tar.bz2 pjproject.tar.bz2 04b2eb1f0f01aa0ad1945b167171843448a51aa6b7c3e806496d434f13a112b7"
)
sha_ok() { [ -s "$1" ] && [ "$(sha256sum < "$1" | cut -d' ' -f1)" = "$2" ]; }
for e in "${FILES[@]}"; do
  read -r url name sha <<< "${e}"
  out=${STORE}/asterisk/${name}
  echo "=== ${url} -> ${out}"
  if ! sha_ok "${out}" "${sha}"; then
    rm -f "${out}"
    if ! { mkdir -p "${out%/*}" && wget -nv -O "${out}.part" "${url}"; }; then
      echo "ERROR: ${url} not fetched"
      RC=1
    elif ! sha_ok "${out}.part" "${sha}"; then
      echo "ERROR: ${url} is not of sha256 ${sha}"
      RC=1
    elif ! mv -f "${out}.part" "${out}"; then
      echo "ERROR: ${out} not written"
      RC=1
    fi
    rm -f "${out}.part"
  fi
  need asterisk/${name}
done
finish "asterisk ${V}, pjproject ${P}"
