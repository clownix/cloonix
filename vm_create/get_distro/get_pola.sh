#!/bin/bash
# Pola PCE (NTT, MIT) amd64 release into the pola tree, under the name
# cnt_kvm_demo/cnt_pola.sh reads, pinned by its sha256: the tree holds that
# file or nothing. 1.3.0: the 1.3.2 and 1.3.3 clients need a topology
# database to send a policy.
. "$(dirname "$0")/distro_common.sh"
V=1.3.0
SHA=c9e8ae792c4e22a9067a3026a6a92529643e6da9b440d027df116572610ca5c8
URL=https://github.com/nttcom/pola/releases/download/v${V}/pola_${V}_linux_amd64.tar.gz
OUT=${STORE}/pola/pola_linux_amd64.tar.gz
sha_ok() { [ -s "$1" ] && [ "$(sha256sum < "$1" | cut -d' ' -f1)" = "${SHA}" ]; }
echo "=== ${URL} -> ${OUT}"
if ! sha_ok "${OUT}"; then
  rm -f "${OUT}"
  if ! { mkdir -p "${OUT%/*}" && wget -nv -O "${OUT}.part" "${URL}"; }; then
    echo "ERROR: ${URL} not fetched"
    RC=1
  elif ! sha_ok "${OUT}.part"; then
    echo "ERROR: ${URL} is not of sha256 ${SHA}"
    RC=1
  elif ! mv -f "${OUT}.part" "${OUT}"; then
    echo "ERROR: ${OUT} not written"
    RC=1
  fi
  rm -f "${OUT}.part"
fi
need pola/pola_linux_amd64.tar.gz
finish "pola ${V}"
