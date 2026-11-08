#!/bin/bash
# Link /var/www/html/<tree> to each tree of the store, then check through the
# web server every file the builders read. Run with sudo.
. "$(dirname "$0")/distro_common.sh"
WWW=/var/www/html
# tree, a file a builder reads in it, the script that mirrors it
NEED=(
  "trixie     dists/trixie/Release                                     get_debian.sh"
  "trixie     dists/trixie-updates/Release                             get_debian.sh"
  "trixie-security dists/trixie-security/Release                       get_debian.sh"
  "resolute   dists/resolute/Release                                   get_ubuntu.sh"
  "resolute   dists/resolute-updates/Release                           get_ubuntu.sh"
  "resolute   dists/resolute-security/Release                          get_ubuntu.sh"
  "rocky      10/BaseOS/x86_64/os/repodata/repomd.xml                  get_rocky.sh"
  "rocky      10/AppStream/x86_64/os/repodata/repomd.xml               get_rocky.sh"
  "fedora     releases/44/Everything/x86_64/os/repodata/repomd.xml     get_fedora.sh"
  "fedora     updates/44/Everything/x86_64/repodata/repomd.xml         get_fedora.sh"
  "fedora     releases/45/Everything/x86_64/os/repodata/repomd.xml     get_fedora.sh"
  "fedora     updates/45/Everything/x86_64/repodata/repomd.xml         get_fedora.sh"
  "alpine     main/x86_64/APKINDEX.tar.gz                              get_alpine.sh"
  "alpine     community/x86_64/APKINDEX.tar.gz                         get_alpine.sh"
  "alpine     releases/x86_64/alpine-minirootfs-latest-x86_64.tar.gz   get_alpine.sh"
  "arch       core/os/x86_64/core.db                                   get_arch.sh"
  "arch       extra/os/x86_64/extra.db                                 get_arch.sh"
  "arch       iso/latest/archlinux-bootstrap-x86_64.tar.zst            get_arch.sh"
  "tumbleweed repo/oss/repodata/repomd.xml                             get_opensuse.sh"
  "openwrt    targets/x86/64/packages/packages.adb                     get_openwrt.sh"
  "openwrt    packages/x86_64/base/packages.adb                        get_openwrt.sh"
  "pola       pola_linux_amd64.tar.gz                                  get_pola.sh"
  "asterisk   asterisk.tar.gz                                          get_asterisk.sh"
  "asterisk   pjproject.tar.bz2                                        get_asterisk.sh"
)
[ -d "${WWW}" ] || { echo "FATAL: ${WWW} does not exist"; exit 1; }
[ -d "${STORE}" ] || { echo "FATAL: ${STORE} not mounted"; exit 1; }
prev=""
for e in "${NEED[@]}"; do
  read -r tree file script <<< "${e}"
  if [ "${tree}" != "${prev}" ]; then
    prev=${tree}
    link=${WWW}/${tree}
    cur=$(readlink "${link}")
    if [ ! -d "${STORE}/${tree}" ]; then
      echo "MISS  ${STORE}/${tree} (run ${script})"
      RC=1
    elif [ -L "${link}" ] && [ "${cur%/}" = "${STORE}/${tree}" ]; then
      echo "OK    ${link} -> ${cur}"
    elif [ -e "${link}" ] && [ ! -L "${link}" ]; then
      echo "FAIL  ${link} exists and is not a symlink"
      RC=1
    elif ln -sfn "${STORE}/${tree}" "${link}"; then
      echo "LINK  ${link} -> ${STORE}/${tree}${cur:+, was ${cur}}"
    else
      echo "FAIL  ${link} not made: ${WWW} belongs to root, run: sudo $0"
      RC=1
    fi
  fi
  url=http://127.0.0.1/${tree}/${file}
  if wget -q --spider "${url}"; then
    echo "OK    ${url}"
  elif [ -e "${WWW}/${tree}/${file}" ]; then
    echo "FAIL  ${url} is on disk, not served"
    RC=1
  elif [ -e "${STORE}/${tree}/${file}" ]; then
    echo "FAIL  ${url}: in ${STORE}/${tree}, ${link} does not lead to it"
    RC=1
  else
    echo "MISS  ${url} (run ${script})"
    RC=1
  fi
done
finish "links of ${WWW} and files the builders read"
