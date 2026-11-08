#!/bin/bash
# Make the local mirror the ONLY repository an rpm build ever reads.

# rpm_local_repo_only <root> "<id>|<name>|<url>"...: repo files cloonix-<id>, same ids for rpm_dnf_repo_args.
rpm_local_repo_only() {
  local root="$1"; shift
  local out spec id rest name url f base n=0

  mkdir -p "${root}/etc/yum.repos.d"
  out="${root}/etc/yum.repos.d/cloonix-local.repo"
  : > "${out}"
  for spec in "$@"; do
    id="${spec%%|*}"
    rest="${spec#*|}"
    name="${rest%%|*}"
    url="${rest#*|}"
    url="${url%/}"
    cat >> "${out}" << EOF
[cloonix-${id}]
name=cloonix local mirror - ${name}
baseurl=${url}/
enabled=1
gpgcheck=0
sslverify=0
skip_if_unavailable=False

EOF
  done

  # Fedora 45 on keeps its repo files in /usr/share/dnf5/repos.d: a file of
  # the same name in /etc/yum.repos.d overrides one entirely.
  for f in "${root}"/usr/share/dnf5/repos.d/*.repo; do
    [ -f "${f}" ] || continue
    [ -e "${root}/etc/yum.repos.d/${f##*/}" ] || cp "${f}" "${root}/etc/yum.repos.d/"
  done
  for f in "${root}"/etc/yum.repos.d/*.repo; do
    [ -f "${f}" ] || continue
    base="${f##*/}"
    [ "${base}" = "cloonix-local.repo" ] && continue
    if grep -q '^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1' "${f}"; then
      sed -i 's/^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1/enabled=0/' "${f}"
      echo "  repo off: ${base}"
      n=$((n+1))
    fi
  done
  # rpm renames a config file it does not own rather than overwriting it.
  rm -f "${root}"/etc/yum.repos.d/*.rpmorig \
        "${root}"/etc/yum.repos.d/*.rpmnew 2>/dev/null
  [ "${n}" != "0" ] && echo "  ${n} non-local repo file(s) disabled"
  return 0
}

# rpm_dnf_repo_args <id>...: pin dnf to the local repos by id.
rpm_dnf_repo_args() {
  local id args="--disablerepo=*"
  for id in "$@"; do
    args="${args} --enablerepo=cloonix-${id}"
  done
  printf '%s' "${args}"
}

# zypp_local_repo_only <root> openSUSE keeps zypper's repositories in
# /etc/zypp/repos.d, which is not where dnf looks.
zypp_local_repo_only() {
  local root="$1" f base
  mkdir -p "${root}/etc/zypp/repos.d"
  cp -f "${root}/etc/yum.repos.d/cloonix-local.repo" \
        "${root}/etc/zypp/repos.d/cloonix-local.repo" 2>/dev/null || return 0
  for f in "${root}"/etc/zypp/repos.d/*.repo; do
    [ -f "${f}" ] || continue
    base="${f##*/}"
    [ "${base}" = "cloonix-local.repo" ] && continue
    if grep -q '^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1' "${f}"; then
      sed -i 's/^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1/enabled=0/' "${f}"
      echo "  zypp repo off: ${base}"
    fi
  done
  if [ -d "${root}/etc/zypp/services.d" ]; then
    for f in "${root}"/etc/zypp/services.d/*; do
      [ -f "${f}" ] || continue
      sed -i 's/^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1/enabled=0/' "${f}"
      echo "  zypp service off: ${f##*/}"
    done
  fi
  return 0
}

# rpm_check_repos_local <root> Say so if anything still enabled points off the
# box.
rpm_check_repos_local() {
  local root="$1" bad f files=()
  for f in "${root}"/etc/yum.repos.d/*.repo "${root}"/etc/zypp/repos.d/*.repo; do
    [ -f "${f}" ] && files+=("${f}")
  done
  for f in "${root}"/usr/share/dnf5/repos.d/*.repo; do
    [ -f "${f}" ] && [ ! -e "${root}/etc/yum.repos.d/${f##*/}" ] && files+=("${f}")
  done
  [ "${#files[@]}" != 0 ] || { echo "FATAL: no repo file in ${root}"; return 1; }
  # Only existing files: mawk stops at a missing one, before its END.
  bad=$(awk '
      function flush() { if (en && net != "") print file": "sect" "net; en=0; net="" }
      FNR == 1 { flush(); file = FILENAME }
      /^[[:space:]]*\[/ { flush(); sect=$0; next }
      /^[[:space:]]*enabled[[:space:]]*=[[:space:]]*1/ { en=1 }
      /^[[:space:]]*(baseurl|metalink|mirrorlist)[[:space:]]*=/ {
        if ($0 !~ /127\.0\.0\.1/ && $0 !~ /10\.117\.0\.1/) net=$0 }
      END { flush() }
    ' "${files[@]}") || { echo "FATAL: the repo files of ${root} could not be read"; return 1; }
  if [ -n "${bad}" ]; then
    echo "FATAL: these enabled repositories are not the local mirror:"
    printf '%s\n' "${bad}" | sed 's/^/         /'
    return 1
  fi
  echo "repos: local mirror only"
  return 0
}
