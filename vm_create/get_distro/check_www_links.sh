#!/bin/bash
# Create the /var/www/html symlinks that the kvm_*.sh image builders expect,
# pointing at the trees the get_*.sh mirror scripts produce.
set -u
STORE="/media/perrier/T7Shield2/distros"
WWW="/var/www/html"
# name -> path under ${STORE}
LINKS=(
  "trixie:trixie"
  "resolute:resolute"
  "rocky:rocky"
  "fedora44:fedora44"
  "alpine:alpine"
  "arch:arch"
  "tumbleweed:tumbleweed"
)
[ -d "${WWW}" ] || { echo "FATAL: ${WWW} does not exist"; exit 1; }
[ -d "${STORE}" ] || { echo "FATAL: ${STORE} not mounted"; exit 1; }
if [ "$(id -u)" != "0" ]; then
  echo "NOTE: not running as root; ln will likely fail on ${WWW}."
  echo "      re-run with sudo if you see permission errors."
fi
rc=0
for entry in "${LINKS[@]}"; do
  name="${entry%%:*}"
  sub="${entry#*:}"
  target="${STORE}/${sub}"
  link="${WWW}/${name}"
  if [ ! -d "${target}" ]; then
    echo "SKIP  ${name}: ${target} does not exist (run get_${name}.sh first)"
    rc=1
    continue
  fi
  if [ -L "${link}" ]; then
    current=$(readlink "${link}")
    if [ "${current%/}" = "${target%/}" ]; then
      echo "OK    ${name} -> ${target}"
      continue
    fi
    echo "FIX   ${name}: was ${current}"
  elif [ -e "${link}" ]; then
    echo "SKIP  ${name}: ${link} exists and is not a symlink"
    rc=1
    continue
  fi
  if ln -sfn "${target}" "${link}"; then
    echo "LINK  ${name} -> ${target}"
  else
    echo "FAIL  ${name}: could not create ${link}"
    rc=1
  fi
done
echo
echo "--- checking the paths the builders require ---"
check() {
  local name="$1" path="$2" what="$3"
  if [ -e "${WWW}/${path}" ]; then
    echo "OK    ${name}: ${what}"
  else
    echo "MISS  ${name}: ${what}"
    echo "        expected ${WWW}/${path}"
    rc=1
  fi
}
check trixie   "trixie/dists/trixie/Release"                  "trixie Release"
check resolute "resolute/dists/resolute/Release"              "resolute Release"
check rocky    "rocky/10/BaseOS/x86_64/os/repodata/repomd.xml"       "BaseOS repodata"
check rocky    "rocky/10/AppStream/x86_64/os/repodata/repomd.xml"    "AppStream repodata"
check fedora   "fedora44/releases/44/Everything/os/repodata/repomd.xml" "Everything repodata"
check fedora   "fedora44/updates/44/Everything/x86_64/repodata/repomd.xml" "updates repodata"
check alpine   "alpine/main/x86_64/APKINDEX.tar.gz"                  "main APKINDEX"
check alpine   "alpine/community/x86_64/APKINDEX.tar.gz"             "community APKINDEX"
check alpine   "alpine/releases/x86_64/alpine-minirootfs-latest-x86_64.tar.gz" "minirootfs tarball"
check arch     "arch/core/os/x86_64/core.db"                         "core.db"
check arch     "arch/extra/os/x86_64/extra.db"                       "extra.db"
check arch     "arch/iso/latest/archlinux-bootstrap-x86_64.tar.zst"  "bootstrap tarball"
check tumbleweed "tumbleweed/repo/oss/repodata/repomd.xml"    "oss repodata"
echo
if [ "${rc}" = "0" ]; then
  echo "DONE: every builder path is present and served."
else
  echo "COMPLETED WITH WARNINGS: some paths are missing (see above)."
  echo "Run the matching get_*.sh for anything reported MISS."
fi
exit ${rc}
