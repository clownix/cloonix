#!/bin/bash
# cnt_pola <rootfs> <url>: Pola PCE (NTT, MIT) polad and pola into
# <rootfs>/usr/local/bin for frr_demo/pathd_pcep.py, from the pola tree of
# the mirror at <url>, which get_distro/get_pola.sh fills.
cnt_pola() {
  local ROOTFS="$1" TGZ="$2/pola_linux_amd64.tar.gz" TMP
  TMP=$(mktemp -d) || return 1
  if ! wget -q -O "${TMP}/pola.tar.gz" "${TGZ}"; then
    echo "ERROR: ${TGZ} not reachable: run get_pola.sh to mirror it, then check_www_links.sh to publish it"
  elif ! { tar -C "${TMP}" -xzf "${TMP}/pola.tar.gz" polad pola LICENSE &&
           mkdir -p "${ROOTFS}/usr/local/bin" "${ROOTFS}/usr/share/doc/pola" &&
           install -m 755 "${TMP}/polad" "${TMP}/pola" "${ROOTFS}/usr/local/bin" &&
           install -m 644 "${TMP}/LICENSE" "${ROOTFS}/usr/share/doc/pola"; }; then
    echo "ERROR: pola not installed in ${ROOTFS}"
  else
    rm -rf "${TMP}"
    echo "  pola in ${ROOTFS}/usr/local/bin"
    return 0
  fi
  rm -rf "${TMP}"
  return 1
}
