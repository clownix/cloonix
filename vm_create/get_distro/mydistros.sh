#!/bin/bash
# Run every get_<distro>.sh of this directory at once, one background
# process per distro in a detached session, their whole output in LOG,
# truncated at each launch. The run uses a copy of this directory made at
# launch: the tree may change, move or go while it lasts.
# Its own lines, tagged [mydistros]:
#   FATAL        disk absent or not writable (nothing launched), or gone
#                during the run (every download stopped)
#   UNREACHABLE  first time a distro could not connect to a host
#   RETRY        a script ending with rc != 0 is run once more
#   ERROR        it failed again; before DONE, the lines of its output
#                that state the problem
#   DONE         ok and failed distros, once all have ended
# ./mydistros.sh          launch, unless a launch is running
# ./mydistros.sh --stop   stop the running launch, killing after 30 s what
#                         a TERM did not end
set -u
SELF=$(readlink -f "$0")
HERE=$(dirname "${SELF}")
. "${HERE}/distro_common.sh"
DISK=${STORE%/*}
LOG="/var/log/mydistros"
LOCK="/tmp/mydistros.lock"
# English messages: network errors are recognised by their wording.
unset LC_ALL
export LC_MESSAGES=C
# Output lines that state a problem.
ERRPAT='(ERROR|WARNING|FATAL)|^rsync(:| error| warning)|file has vanished|[Ff]ailed( to|:)|Errors:|[Cc]an.t |No such file|[Pp]ermission denied|Read-only file system|No space left|command not found|Input/output error|debmirror line [0-9]'
# No connection made: a reset comes once one was.
NET='failed to connect|[Cc]onnection (timed out|refused)|[Nn]o route to host|[Nn]etwork is unreachable|[Nn]ame or service not known|[Tt]emporary failure in name resolution|unable to resolve|[Cc]ould not resolve|error in socket IO|max connections|timeout waiting for daemon|[Cc]an.t connect to'

log() { printf '%(%F %T)T [mydistros] %s\n' -1 "$*"; }

# One log line per output line: \r progress updates become lines, \b
# overstrikes are applied as a terminal would. wget tries each address in
# turn, a failing one printed "Connecting to ... failed: <why>", or under -nv
# "failed: <why>": one address failing is no verdict.
prefix() {
  local name=$1 line prev flagged=0
  while IFS= read -r line; do
    while [[ ${line} == *$'\b'* ]]; do
      prev=${line}
      line=${line//[^$'\b']$'\b'/}
      [ "${line}" = "${prev}" ] && line=${line//$'\b'/}
    done
    printf '%(%F %T)T [%s] %s\n' -1 "${name}" "${line}"
    if [ "${flagged}" = 0 ] && [[ ${line} =~ ${NET} ]] &&
       [[ ${line} != "Connecting to "* && ${line} != "failed: "* ]]; then
      log "UNREACHABLE ${name}: ${line}"
      flagged=1
    fi
  done
}

run_one() {
  local name=$1 script=$2 rc run all="" stopping=0
  # Outlives a stop signal, so as to reap its pipeline and log the rc.
  trap 'stopping=1' TERM INT HUP
  log "START ${name}: ${FROM}/${script##*/}"
  for run in 1 2; do
    "${script}" < /dev/null 2>&1 | stdbuf -oL tr '\r' '\n' | prefix "${name}"
    rc=${PIPESTATUS[0]}
    all="${all:+${all}, then }rc=${rc}"
    if [ "${rc}" = 0 ] || [ "${stopping}" = 1 ] || [ "${run}" = 2 ]; then break; fi
    log "RETRY ${name}: ${script##*/} ended with rc=${rc}, run once more"
  done
  if [ "${rc}" = 0 ]; then
    log "END ${name}: ok$([ "${run}" = 2 ] && echo ' at the second run')"
  else
    log "ERROR ${name}: ${script##*/} ended with ${all}"
  fi
  return "${rc}"
}

# The problem lines of a failed distro, a repeated one once with its count,
# the first and last 10 of more than 20; its last 5 lines when none matches.
why() {
  local name=$1 lines l
  lines=$(grep -F " [${name}] " "${LOG}" | sed 's/^[^]]*\] //' \
          | grep -E "${ERRPAT}" | grep -v -e '^COMPLETED WITH' -e '^Connecting to ' \
          | awk '{ if (!($0 in n)) o[++k] = $0; n[$0]++ }
                 END { for (i = 1; i <= k; i++) print (n[o[i]] > 1 ? "(x" n[o[i]] ") " : "") o[i] }' \
          | awk 'NR<=10 {print; next} {buf[NR%10]=$0}
                 END {if (NR>20) print "... " NR-20 " more such lines above";
                      for (i=(NR>20)?NR-9:11; i<=NR; i++) print buf[i%10]}')
  [ -n "${lines}" ] || lines=$(grep -F " [${name}] " "${LOG}" | sed 's/^[^]]*\] //' | tail -n 5)
  while IFS= read -r l; do log "ERROR ${name}: ${l}"; done <<< "${lines}"
}

stop() {
  trap '' TERM INT HUP
  log "$1"
  kill -TERM 0
  wait
  log "STOPPED: every download killed"
  rm_copy
  exit 1
}

rm_copy() { [[ ${HERE} == /tmp/mydistros_run.* ]] && rm -rf "${HERE}"; }

# The processes holding the lock open.
holders() {
  local f p
  for f in /proc/[0-9]*/fd/*; do
    [ "$(readlink "${f}" 2>/dev/null)" = "${LOCK}" ] || continue
    p=${f#/proc/}
    ps -o pid=,stat=,etime=,args= -p "${p%%/*}"
  done | sort -u
}

# The lock taken for an instant only: free means no launch running.
free() { flock -n "${LOCK}" true; }

# --stop: TERM to the session of the running launch, KILL to what is left
# after 30 s, done once the lock is free.
stop_launch() {
  local pid i
  free && { echo "no launch running"; exit 0; }
  pid=$(cut -d' ' -f1 "${LOCK}")
  echo "stopping the launch of $(cat "${LOCK}")"
  if ! kill -TERM -- "-${pid}"; then
    echo "FATAL: session ${pid} is gone, the lock is held by:"
    holders
    exit 1
  fi
  for i in $(seq 30); do
    sleep 1
    free && { echo "stopped"; exit 0; }
  done
  log "WARNING: --stop: still running after 30 s, killed:" | tee -a "${LOG}"
  holders | tee -a "${LOG}"
  kill -KILL -- "-${pid}"
  for i in $(seq 5); do
    sleep 1
    free && { rm -rf /tmp/mydistros_run.*; echo "killed"; exit 0; }
  done
  echo "FATAL: the lock is still held, by processes a KILL cannot end (D: disk I/O):"
  holders
  exit 1
}

if [ -n "${MYDISTROS_DAEMON:-}" ]; then
  FROM=${MYDISTROS_DAEMON}
  unset MYDISTROS_DAEMON
  trap 'stop "STOP: signal received, stopping every download"' TERM INT HUP
  names=()
  pids=()
  for script in "${HERE}"/get_*.sh; do
    name=${script##*/get_}
    names+=("${name%.sh}")
  done
  log "LAUNCH session $$ from ${FROM}, run from its copy ${HERE}: ${names[*]}"
  for i in "${!names[@]}"; do
    run_one "${names[$i]}" "${HERE}/get_${names[$i]}.sh" &
    pids+=($!)
  done
  # A vanished disk would leave the scripts writing elsewhere.
  while :; do
    alive=0
    for p in "${pids[@]}"; do kill -0 "${p}" 2>/dev/null && alive=1; done
    [ "${alive}" = 0 ] && break
    mountpoint -q "${DISK}" || stop "FATAL: ${DISK} is no longer mounted, stopping every download"
    sleep 10
  done
  ok=()
  bad=()
  for i in "${!pids[@]}"; do
    if wait "${pids[$i]}"; then ok+=("${names[$i]}"); else bad+=("${names[$i]}"); fi
  done
  for name in "${bad[@]}"; do why "${name}"; done
  log "DONE: ok: ${ok[*]:-none}; failed: ${bad[*]:-none}"
  rm_copy
  exit $(( ${#bad[@]} > 0 ))
fi

[ "$(id -u)" != 0 ] || { echo "FATAL: run as your user, sudo only creates ${LOG}"; exit 1; }
case "${1:-}" in
  "") ;;
  --stop) stop_launch ;;
  *) echo "usage: ${0##*/} [--stop]"; exit 1 ;;
esac
# The lock before the log: a refused launch leaves the running one's log alone.
exec 9>>"${LOCK}"
if ! flock -n 9; then
  echo "FATAL: the launch of $(cat "${LOCK}") is still running; follow it with:"
  echo "tail -f ${LOG}"
  echo "stop it with:"
  echo "${SELF} --stop"
  exit 1
fi
if [ ! -w "${LOG}" ]; then
  sudo install -m 644 -o "$(id -un)" -g "$(id -gn)" /dev/null "${LOG}" || \
    { echo "FATAL: ${LOG} cannot be created"; exit 1; }
fi
: > "${LOG}"
if ! mountpoint -q "${DISK}"; then
  log "FATAL: ${DISK} is not mounted, nothing launched" | tee -a "${LOG}"
  exit 1
fi
if [ ! -w "${DISK}" ]; then
  log "FATAL: ${DISK} is not writable, nothing launched" | tee -a "${LOG}"
  exit 1
fi
scripts=("${HERE}"/get_*.sh)
[ -e "${scripts[0]}" ] || { log "FATAL: no get_*.sh in ${HERE}" | tee -a "${LOG}"; exit 1; }
COPY=$(mktemp -d /tmp/mydistros_run.XXXXXX) && cp -a "${HERE}/." "${COPY}/" || \
  { log "FATAL: ${HERE} cannot be copied into /tmp, nothing launched" | tee -a "${LOG}"; exit 1; }
# fd 9 and its lock pass to the session: no second launch until it ends.
MYDISTROS_DAEMON="${HERE}" setsid "${COPY}/${SELF##*/}" < /dev/null >> "${LOG}" 2>&1 &
pid=$!
echo "${pid} ($(date '+%F %T'))" > "${LOCK}"
echo "${#scripts[@]} downloads running in session ${pid}; follow them with:"
echo "tail -f ${LOG}"
echo "stop them all with:"
echo "${SELF} --stop"
