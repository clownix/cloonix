#!/bin/bash
# cnt_pola <rootfs>: Pola PCE (NTT, MIT) polad and pola into
# <rootfs>/usr/local/bin for frr_demo/pathd_pcep.py; 1.3.0 because the 1.3.2
# and 1.3.3 clients need a topology database to send a policy.
cnt_pola() {
  local ROOTFS="$1" V="1.3.0" A SHA TGZ TMP
  case "$(uname -m)" in
    x86_64)  A="amd64"
             SHA="c9e8ae792c4e22a9067a3026a6a92529643e6da9b440d027df116572610ca5c8" ;;
    aarch64) A="arm64"
             SHA="1b1312f4bf2f0b22ff21830cfdd2ce16f16282e770511e904a7c413fb0df09a8" ;;
    *) echo "ERROR: no pola release for $(uname -m)"; return 1 ;;
  esac
  TGZ="pola_${V}_linux_${A}.tar.gz"
  TMP=$(mktemp -d)
  wget -q -O ${TMP}/${TGZ} \
    https://github.com/nttcom/pola/releases/download/v${V}/${TGZ}
  if ! echo "${SHA}  ${TMP}/${TGZ}" | sha256sum -c --quiet; then
    echo "ERROR: ${TGZ} not fetched or with a wrong sha256"
    rm -rf ${TMP}
    return 1
  fi
  tar -C ${TMP} -xzf ${TMP}/${TGZ} polad pola LICENSE
  mkdir -p ${ROOTFS}/usr/local/bin ${ROOTFS}/usr/share/doc/pola
  install -m 755 ${TMP}/polad ${TMP}/pola ${ROOTFS}/usr/local/bin
  install -m 644 ${TMP}/LICENSE ${ROOTFS}/usr/share/doc/pola
  rm -rf ${TMP}
  echo "  pola ${V} in ${ROOTFS}/usr/local/bin"
}
