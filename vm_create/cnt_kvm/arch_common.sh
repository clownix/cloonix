#!/bin/bash
# Checks shared by cnt_arch.sh and kvm_arch.sh.

# arch_check_pkgs_served <chroot-bin> <bootstrap-root> <target-root> <pkgs>: every resolved file must be on the mirror.
ARCH_SPIDER_JOBS="${ARCH_SPIDER_JOBS:-16}"

arch_check_pkgs_served() {
  local chroot_bin="$1" broot="$2" troot="$3" pkgs="$4"
  local out rc uris n total tmpd missing cpath

  cpath="PATH=/usr/sbin:/usr/bin:/sbin:/bin"
  command -v wget >/dev/null 2>&1 || return 0

  # alpm needs an existing dbpath.
  mkdir -p "${broot}${troot}/var/lib/pacman" \
           "${broot}${troot}/var/cache/pacman/pkg" 2>/dev/null

  # NOT --disable-download-timeout.
  echo "checking the mirror serves everything this build needs..."
  out=$("${chroot_bin}" "${broot}" sh -c \
        "${cpath}; pacman --root ${troot} --noconfirm -Syp ${pkgs}" 2>&1)
  rc=$?
  if [ "${rc}" != "0" ]; then
    echo "ERROR: pacman could not resolve the package list (rc=${rc})."
    echo "${out}" | tail -20 | sed 's/^/       /'
    echo
    case "${out}" in
      *"failed to initialize alpm"*)
        echo "       alpm could not open its database directory under"
        echo "       ${troot}. That is a directory this script should have"
        echo "       created, not a mirror or a package problem." ;;
      *"target not found"*)
        echo "       The databases are fine and the name is simply not in"
        echo "       core or extra. Arch is rolling and drops packages --"
        echo "       rsyslog left for the AUR in 2018, which is why these"
        echo "       lists say syslog-ng. Check the name at"
        echo "       archlinux.org/packages and fix PKGS in this script." ;;
      *"could not resolve host"*|*"Connection refused"*|*"404"*)
        echo "       The mirror did not answer or is missing a database."
        echo "       Re-run vm_create/get_distro/get_arch.sh." ;;
      *)
        echo "       Read the pacman lines above: a name it cannot find is"
        echo "       this script's PKGS list, anything else is the mirror"
        echo "       (re-run vm_create/get_distro/get_arch.sh)." ;;
    esac
    return 1
  fi

  uris=$(echo "${out}" | grep -E '^(https?|file)://')
  if [ -z "${uris}" ]; then
    echo "note: pacman printed no download URIs, nothing to pre-check."
    return 0
  fi

  # In parallel, with a timeout, and after saying how many there are.
  tmpd=$(mktemp -d)
  printf '%s\n' ${uris} | grep -v '^file://' > "${tmpd}/uris"
  total=$(grep -c . "${tmpd}/uris")
  [ "${total}" = "0" ] && { rm -rf "${tmpd}"; \
    echo "mirror OK: nothing to fetch"; return 0; }
  echo "  ${total} file(s), ${ARCH_SPIDER_JOBS} at a time, a few seconds"
  xargs -a "${tmpd}/uris" -r -n1 -P "${ARCH_SPIDER_JOBS}" sh -c '
      wget -q --spider --timeout=20 --tries=2 "$1" 2>/dev/null || \
        printf "%s\n" "$1"
    ' _ > "${tmpd}/missing" 2>/dev/null
  n=$(grep -c . "${tmpd}/missing")

  if [ "${n}" = "0" ]; then
    rm -rf "${tmpd}"
    echo "mirror OK: every package of this build is served"
    return 0
  fi

  missing=$(cat "${tmpd}/missing")
  rm -rf "${tmpd}"
  echo "ERROR: the mirror does not serve ${n} package(s) this build needs."
  echo
  printf '%s\n' "${missing}" | head -10 | sed 's/^/         /'
  [ "${n}" -gt 10 ] && echo "         ... and $((n-10)) more"
  echo
  case "${missing}" in
    *:*-*)
      echo "       A name with an epoch in it -- the '1:' in"
      echo "       nftables-1:1.1.6-3 -- cannot be stored on exFAT or NTFS,"
      echo "       because ':' is not a legal filename character there."
      echo "       get_arch.sh works around that by fetching such packages"
      echo "       under a colon-free name and rewriting the database to"
      echo "       match, so one that still has a colon here is one that"
      echo "       fixup did not manage."
      echo ;;
  esac
  echo "       Re-run vm_create/get_distro/get_arch.sh and read what it"
  echo "       prints per repo: which script file is running (there is more"
  echo "       than one copy of these on disk), the destination filesystem,"
  echo "       how many epoch packages it fixed, and any it could not."
  echo
  echo "       Stopping now rather than downloading the rest and aborting"
  echo "       on this with nothing installed."
  return 1
}
