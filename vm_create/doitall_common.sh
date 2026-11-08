#!/bin/bash
# The run of a doitall.sh: every build tried in turn in a detached session,
# from a copy of vm_create made at launch; the whole output, then a report
# of the failures, in /var/log/doitall_<dir>.log. What a failed build leaves
# is erased: an image left in bulk is a valid one.
# The doitall.sh sets HERE, then calls dc_launch with DOITALL_BUILDS, items
# kind:name or kvm:name:base (a desktop and the qcow2 it starts from),
# DOITALL_ERASE_KVM=1 to erase a qcow2 and its .prev before its build, and
# DOITALL_WAIT=1 to follow the run and return its status.
# Its own lines, tagged [doitall]: START, END, ERROR of each build, WAIT for
# the other doitall, then REPORT, the ERROR lines of each failure, the
# WARNING lines the ok builds went on with, IMAGE and DONE.

BULK="/var/lib/cloonix/bulk"
DC_VMC=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
DIRNAME="${HERE##*/}"
LOG="/var/log/doitall_${DIRNAME}.log"
OWN_LOCK="/tmp/doitall_${DIRNAME}.lock"
ALL_LOCK="/tmp/doitall.lock"
SUDO_PAT='(^|[;&|]|\$\(|`|^[[:space:]]*(then|else|do)[[:space:]]+)[[:space:]]*sudo[[:space:]]'
# Output lines that state a problem.
ERRPAT='(ERROR|WARNING|FATAL|MISSING)|[Ff]ailed( to|:)|not reachable|No such file|[Pp]ermission denied|Read-only file system|No space left|command not found|Input/output error|[Cc]annot |[Cc]ould not'

log() { printf '%(%F %T)T [%s] %s\n' -1 "${DC_TAG:-doitall}" "$*"; }

dur() { printf '%dh%02dm%02ds' $(($1 / 3600)) $(($1 % 3600 / 60)) $(($1 % 60)); }

# One log line per output line: \r progress updates become lines, \b
# overstrikes are applied as a terminal would.
prefix() {
  local tag="$1" line prev
  while IFS= read -r line; do
    while [[ ${line} == *$'\b'* ]]; do
      prev=${line}
      line=${line//[^$'\b']$'\b'/}
      [ "${line}" = "${prev}" ] && line=${line//$'\b'/}
    done
    printf '%(%F %T)T [%s] %s\n' -1 "${tag}" "${line}"
  done
}

# Output of the build <tag>, stamps off.
body() { grep -F " [$1] " "${LOG}" | sed 's/^[^]]*\] //'; }

# The problem lines of a failed build, a repeated one once with its count,
# the first and last 10 of more than 20, then its last lines not among them.
dc_why() {
  local tag="$1" probs l
  probs=$(body "${tag}" | grep -E "${ERRPAT}")
  { [ -n "${probs}" ] && printf '%s\n' "${probs}" \
      | awk '{ if (!($0 in n)) o[++k] = $0; n[$0]++ }
             END { for (i = 1; i <= k; i++) print (n[o[i]] > 1 ? "(x" n[o[i]] ") " : "") o[i] }' \
      | awk 'NR<=10 {print; next} {buf[NR%10]=$0}
             END {if (NR>20) print "... " NR-20 " more such lines above";
                  for (i=(NR>20)?NR-9:11; i<=NR; i++) print buf[i%10]}'
    body "${tag}" | tail -n 5 | grep -v -x -F -f <(printf '%s\n' "${probs}") | sed 's/^/last: /'
  } | while IFS= read -r l; do log "ERROR ${tag}: ${l}"; done
}

# What the ok builds went on without, a warning given by several builds once,
# with their names.
dc_warnings() {
  local tag t l
  for tag in "$@"; do
    body "${tag}" | grep -E '^ *WARNING' | sed 's/^ *//' | awk -v t="${tag}" '!seen[$0]++ { print t "\t" $0 }'
  done | awk -F '\t' '{ if (!($2 in tags)) o[++k] = $2; tags[$2] = tags[$2] (tags[$2] ? " " : "") $1 }
                      END { for (i = 1; i <= k; i++) print tags[o[i]] "\t" o[i] }' \
       | while IFS=$'\t' read -r t l; do log "WARNING ${t}: ${l}"; done
}

# Erase what a build of <name> replaces; the reason on stdout when it cannot.
dc_erase() {
  local kind="$1" name="$2" t="${BULK}/$2" q
  case "${name}" in
    ""|*/*|.|..) echo "FATAL: refusing to erase for '${name}'"; return 1 ;;
  esac
  if [ "${kind}" = "cnt" ]; then
    if [ -e "${t}" ]; then
      echo "erasing previous container tree ${t}"
      unshare --map-root-user --user --map-auto -- rm -rf "${t}" || rm -rf "${t}"
      [ -e "${t}" ] && { echo "FATAL: could not erase ${t}"; return 1; }
    fi
    if [ -e "${t}.tar.gz" ]; then
      echo "erasing previous image ${t}.tar.gz"
      rm -f "${t}.tar.gz"
    fi
    if [ -d "${t}_targz" ]; then
      echo "erasing previous extracted tree ${t}_targz"
      rm -rf "${t}_targz"
      [ -e "${t}_targz" ] && echo "WARNING ${t}_targz belongs to another user and stays: a net using it keeps the old image"
    fi
  elif [ "${DOITALL_ERASE_KVM}" = "1" ]; then
    for q in "${t}.qcow2" "${t}.qcow2.prev"; do
      [ -e "${q}" ] || continue
      echo "erasing previous VM image ${q}"
      rm -f "${q}"
      [ -e "${q}" ] && { echo "FATAL: could not erase ${q}"; return 1; }
    done
  fi
  return 0
}

# What a failed build started at <t0> leaves is no image: a container tree or
# tar.gz, a qcow2 written since <t0>. An older qcow2 it did not touch stays.
dc_unbuild() {
  local kind="$1" name="$2" t0="$3" t="${BULK}/$2" f
  if [ "${kind}" = "cnt" ]; then
    set -- "${t}" "${t}.tar.gz"
  else
    set --
    [ -e "${t}.qcow2" ] && [ "$(stat -c %Y "${t}.qcow2")" -ge "${t0}" ] && set -- "${t}.qcow2"
  fi
  for f in "$@"; do
    [ -e "${f}" ] || continue
    unshare --map-root-user --user --map-auto -- rm -rf "${f}" || rm -rf "${f}"
    if [ -e "${f}" ]; then
      log "WARNING ${kind}_${name}: ${f}, left by the failed build, could not be erased"
    else
      log "ERASED ${kind}_${name}: ${f}, left by the failed build"
      UNBUILT[${kind}_${name}]="${UNBUILT[${kind}_${name}]:-}${UNBUILT[${kind}_${name}]:+ }${f}"
    fi
  done
}

# Checks and erase before a build; the reason on stdout when it cannot run.
dc_ready() {
  local kind="$1" name="$2" base="$3" script="$4"
  [ -f "${script}" ] || { echo "MISSING: ${FROM}/${script##*/}"; return 1; }
  [ -x "${script}" ] || { echo "MISSING: ${FROM}/${script##*/} is not executable"; return 1; }
  if grep -qE "${SUDO_PAT}" "${script}"; then
    echo "FATAL: ${script##*/} calls sudo, these builders work without root:"
    grep -nE "${SUDO_PAT}" "${script}" | head -3
    return 1
  fi
  if [ -n "${base}" ] && [ ! -f "${BULK}/${base}.qcow2" ]; then
    echo "MISSING: ${BULK}/${base}.qcow2, the image this desktop starts from"
    return 1
  fi
  dc_erase "${kind}" "${name}"
}

# One build, in turn; its output tagged <kind>_<name>.
dc_build() {
  local item="$1" kind name base rest tag script t0 rc
  kind=${item%%:*}
  rest=${item#*:}
  name=${rest%%:*}
  base=""
  [ "${rest}" != "${name}" ] && base=${rest#*:}
  tag="${kind}_${name}"
  script="${HERE}/${tag}.sh"
  CUR="${tag}"
  t0=$(date +%s)
  T0[${tag}]=${t0}
  log "START ${tag}: ${FROM}/${tag}.sh"
  dc_ready "${kind}" "${name}" "${base}" "${script}" 2>&1 | prefix "${tag}"
  if [ "${PIPESTATUS[0]}" != "0" ]; then
    rc=1
  else
    (cd "${HERE}" && "${script}") < /dev/null 8<&- 9<&- 2>&1 \
      | stdbuf -oL tr '\r' '\n' | prefix "${tag}"
    rc=${PIPESTATUS[0]}
    if [ "${rc}" = "0" ] && [ "${kind}" = "cnt" ] && \
       { [ ! -f "${BULK}/${name}.tar.gz" ] || [ -e "${BULK}/${name}" ]; }; then
      echo "ERROR: ${tag}.sh exited 0 without delivering ${BULK}/${name}.tar.gz" | prefix "${tag}"
      rc=1
    fi
  fi
  CUR=""
  DUR[${tag}]=$(( $(date +%s) - t0 ))
  if [ "${rc}" = "0" ]; then
    OK+=("${tag}")
    log "END ${tag}: ok in $(dur "${DUR[${tag}]}")"
  else
    BAD+=("${tag}")
    RC[${tag}]=${rc}
    log "ERROR ${tag}: rc=${rc} after $(dur "${DUR[${tag}]}")"
    dc_unbuild "${kind}" "${name}" "${t0}"
  fi
}

dc_report() {
  local tag f
  log "REPORT: ${#OK[@]} ok, ${#BAD[@]} failed, in $(dur $(( $(date +%s) - START )))"
  for tag in "${BAD[@]}"; do
    log "ERROR ${tag}: rc=${RC[${tag}]} after $(dur "${DUR[${tag}]}")"
    dc_why "${tag}"
    [ -n "${UNBUILT[${tag}]:-}" ] && log "ERASED ${tag}: ${UNBUILT[${tag}]}, left by the failed build"
  done
  dc_warnings "${OK[@]}"
  for tag in "${OK[@]}"; do
    case "${tag}" in
      cnt_*) f="${BULK}/${tag#cnt_}.tar.gz" ;;
      *)     f="${BULK}/${tag#kvm_}.qcow2" ;;
    esac
    [ -f "${f}" ] && log "IMAGE ${tag}: ${f} $(du -sh "${f}" | cut -f1)"
  done
  [ -n "${NOTRUN}" ] && log "NOT RUN: ${NOTRUN}"
  log "DONE: ok: ${OK[*]:-none}; failed: ${BAD[*]:-none}"
}

rm_copy() { [[ ${DC_COPY:-} == /tmp/doitall_run.* ]] && rm -rf "${DC_COPY}"; }

dc_stop() {
  trap '' TERM INT HUP
  log "STOP: signal received, the build running is killed"
  kill -TERM 0
  wait
  if [ -n "${CUR}" ]; then
    BAD+=("${CUR}")
    RC[${CUR}]=143
    DUR[${CUR}]=$(( $(date +%s) - T0[${CUR}] ))
    dc_unbuild "${CUR%%_*}" "${CUR#*_}" "${T0[${CUR}]}"
  fi
  dc_report
  rm_copy
  exit 1
}

dc_daemon() {
  local i items
  FROM="${DOITALL_FROM}"
  START=$(date +%s)
  OK=()
  BAD=()
  CUR=""
  NOTRUN=""
  declare -gA RC DUR T0 UNBUILT
  trap dc_stop TERM INT HUP
  # One doitall at a time on this machine: they share bulk and its lock.
  exec 8<"${ALL_LOCK}"
  if ! flock -n 8; then
    log "WAIT: another doitall is running, this one starts when it ends"
    flock 8
  fi
  log "LAUNCH session $$ from ${FROM}, run from its copy ${DC_COPY}: ${DOITALL_BUILDS}"
  read -r -a items <<< "${DOITALL_BUILDS}"
  for i in "${!items[@]}"; do
    NOTRUN="${items[*]:i+1}"
    dc_build "${items[${i}]}"
  done
  NOTRUN=""
  dc_report
  rm_copy
  exit $(( ${#BAD[@]} > 0 ))
}

# A lock file any user can open: flock needs no write access.
dc_lockfile() { [ -e "$1" ] || { : > "$1" && chmod 644 "$1"; }; }

# A log in /var/log, made the user's once by sudo.
dc_logfile() {
  [ -w "$1" ] && return 0
  sudo install -m 644 -o "$(id -un)" -g "$(id -gn)" /dev/null "$1" || \
    { echo "FATAL: $1 cannot be created"; return 1; }
}

# dc_detach <label> <log> <lock> <entry> [VAR=value ...]: <entry>, a path
# under vm_create, started in a detached session from a copy of vm_create
# made now, its output in <log>, emptied; <lock> refuses a second launch.
# The tree may change, move or go while it runs, and the demo builders read
# ../cnt_kvm.
dc_detach() {
  local label="$1" logf="$2" lock="$3" entry="$4" copy
  shift 4
  [ "$(id -u)" != "0" ] || { echo "FATAL: run as your user: a root-owned tree in ${BULK} would block later runs"; exit 1; }
  dc_logfile "${logf}" || exit 1
  dc_lockfile "${lock}" || { echo "FATAL: no lock file ${lock}"; exit 1; }
  exec 9<"${lock}"
  if ! flock -n 9; then
    echo "FATAL: ${label%%:*} is already running; follow it with:"
    echo "tail -f ${logf}"
    exit 1
  fi
  : > "${logf}"
  copy=$(mktemp -d /tmp/doitall_run.XXXXXX) && cp -a "${DC_VMC}/." "${copy}/" || \
    { echo "FATAL: ${DC_VMC} cannot be copied into /tmp, nothing launched"; exit 1; }
  # fd 9 and its lock pass to the session: no second launch until it ends.
  env "$@" DC_COPY="${copy}" DC_ORIGIN="${DC_ORIGIN:-${DC_VMC}}" \
    setsid "${copy}/${entry}" < /dev/null >> "${logf}" 2>&1 &
  DC_PID=$!
  echo "${label} running in session ${DC_PID}; follow it with:"
  echo "tail -f ${logf}"
  echo "stop it with:"
  echo "kill -- -${DC_PID}"
}

# What every build needs of this machine.
dc_preflight() {
  local t
  [ -d "${BULK}" ] || { echo "FATAL: ${BULK} does not exist"; return 1; }
  [ -w "${BULK}" ] || { echo "FATAL: ${BULK} is not writable by $(id -un)"; return 1; }
  for t in unshare flock mkfs.ext4 qemu-img stdbuf; do
    command -v "${t}" >/dev/null 2>&1 || [ -x "/usr/sbin/${t}" ] || \
      { echo "FATAL: ${t} not found"; return 1; }
  done
}

dc_launch() {
  [ -n "${DOITALL_BUILDS}" ] || { echo "nothing to build."; exit 1; }
  dc_preflight || exit 1
  dc_lockfile "${ALL_LOCK}" || { echo "FATAL: no lock file ${ALL_LOCK}"; exit 1; }
  dc_detach "doitall of ${DIRNAME}: $(wc -w <<< "${DOITALL_BUILDS}") builds" \
            "${LOG}" "${OWN_LOCK}" "${DIRNAME}/doitall.sh" \
            DOITALL_DAEMON=1 DOITALL_FROM="${DC_ORIGIN:-${DC_VMC}}/${DIRNAME}" \
            DOITALL_BUILDS="${DOITALL_BUILDS}" DOITALL_ERASE_KVM="${DOITALL_ERASE_KVM:-0}"
  if [ "${DOITALL_WAIT:-0}" = "1" ]; then
    tail -n +1 -f --pid="${DC_PID}" "${LOG}"
    wait "${DC_PID}"
    exit $?
  fi
  exit 0
}
