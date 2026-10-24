#!/bin/bash
# Create the /var/www/html symlinks that the kvm_*.sh image builders expect,
# pointing at the trees the get_*.sh mirror scripts produce.
set -u
STORE="/media/perrier/T7Shield2/distros"
WWW="/var/www/html"
# link name : path under ${STORE} : mirror script
LINKS=(
  "trixie:trixie:get_trixie.sh"
  "resolute:resolute:get_resolute.sh"
  "rocky:rocky:get_rocky.sh"
  "fedora44:fedora44:get_fedora.sh"
  "alpine:alpine:get_alpine.sh"
  "arch:arch:get_arch.sh"
  "tumbleweed:opensuse/tumbleweed:get_opensuse.sh"
)
[ -d "${WWW}" ] || { echo "FATAL: ${WWW} does not exist"; exit 1; }
[ -d "${STORE}" ] || { echo "FATAL: ${STORE} not mounted"; exit 1; }
if [ "$(id -u)" != "0" ]; then
  echo "NOTE: not running as root; ln will likely fail on ${WWW}."
  echo "      re-run with sudo if you see permission errors."
fi
get_of() {
  local e
  for e in "${LINKS[@]}"; do
    [ "${e%%:*}" = "$1" ] && { echo "${e##*:}"; return; }
  done
}
rc=0
for entry in "${LINKS[@]}"; do
  name="${entry%%:*}"
  rest="${entry#*:}"
  sub="${rest%%:*}"
  script="${rest#*:}"
  target="${STORE}/${sub}"
  link="${WWW}/${name}"
  if [ ! -d "${target}" ]; then
    echo "SKIP  ${name}: ${target} does not exist (run ${script} first)"
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
  if [ -e "${WWW}/${name}/${path}" ]; then
    echo "OK    ${name}: ${what}"
  else
    echo "MISS  ${name}: ${what} (run $(get_of "${name}"))"
    echo "        expected ${WWW}/${name}/${path}"
    rc=1
  fi
}
check trixie     "dists/trixie/Release"                            "trixie Release"
check resolute   "dists/resolute/Release"                          "resolute Release"
check rocky      "10/BaseOS/x86_64/os/repodata/repomd.xml"         "BaseOS repodata"
check rocky      "10/AppStream/x86_64/os/repodata/repomd.xml"      "AppStream repodata"
check fedora44   "releases/44/Everything/x86_64/os/repodata/repomd.xml" "Everything repodata"
check fedora44   "updates/44/Everything/x86_64/repodata/repomd.xml" "updates repodata"
check alpine     "main/x86_64/APKINDEX.tar.gz"                     "main APKINDEX"
check alpine     "community/x86_64/APKINDEX.tar.gz"                "community APKINDEX"
check alpine     "releases/x86_64/alpine-minirootfs-latest-x86_64.tar.gz" "minirootfs tarball"
check arch       "core/os/x86_64/core.db"                          "core.db"
check arch       "extra/os/x86_64/extra.db"                        "extra.db"
check arch       "iso/latest/archlinux-bootstrap-x86_64.tar.zst"   "bootstrap tarball"
check tumbleweed "repo/oss/repodata/repomd.xml"                    "oss repodata"
echo
if [ "${rc}" = "0" ]; then
  echo "DONE: every builder path is present and served."
else
  echo "COMPLETED WITH WARNINGS: some paths are missing (see above)."
fi
exit ${rc}
