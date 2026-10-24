#!/bin/bash
# Sourced. pack_toy <parent> <name>: <parent>/<name>.tar.xz, directories
# first, files by name (bin/ libraries beside their root twins for xz);
# -9e single thread: 674 MiB to pack, 65 MiB to unpack.
pack_toy()
{
  local parent="$1" name="$2" list kb
  if ! command -v xz >/dev/null; then
    echo "  TOY-PACK KERR ${name}: xz is not installed (package xz-utils)"
    return 1
  fi
  list=$(mktemp)
  ( cd "${parent}" && find "${name}" -type d | LC_ALL=C sort > "${list}" && \
    find "${name}" ! -type d -printf '%f\t%p\n' | LC_ALL=C sort | \
      cut -f2- >> "${list}" )
  kb=$(du -sk "${parent}/${name}" | cut -f1)
  if ! ( set -o pipefail
         tar -C "${parent}" --no-recursion -cf - -T "${list}" | \
           xz -9e -T1 > "${parent}/${name}.tar.xz" ); then
    echo "  TOY-PACK KERR ${name}: tar or xz failed"
    rm -f "${list}" "${parent}/${name}.tar.xz"
    return 1
  fi
  echo "  TOY-PACK ${name}: $(wc -l < "${list}") entries, ${kb} kB unpacked," \
       "$(stat -c %s "${parent}/${name}.tar.xz") bytes in ${name}.tar.xz"
  rm -f "${list}"
}
