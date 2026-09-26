#!/bin/bash

NET=${1:-nemo}
MINUTES=${2:-6}

# The marker THIS round needs, and where it lives.
PEER=${3:-mito}
NEED_BIN=/usr/libexec/cloonix/cloonfs/bin/cloonix-main-server
NEED_STR="and its agent has not"
NEED_WHAT="the switch (built unstripped, so backtraces name lines)"
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=/tmp/test_round
START=$(date "+%Y-%m-%d %H:%M:%S")

rm -rf ${OUT}
mkdir -p ${OUT}

echo "=========================================================="
echo "ROUND: the c2c that brings the server down"
echo "net ${NET}, peer ${PEER}, up to ${MINUTES} minutes"
echo "=========================================================="
echo

# Is the installed binary this round's?
if [ ! -x "${NEED_BIN}" ]; then
  echo "cannot find ${NEED_BIN}"
  exit 1
fi
if ! strings "${NEED_BIN}" 2>/dev/null | grep -q "${NEED_STR}"; then
  echo "THE INSTALLED BINARY IS NOT THIS ROUND'S BUILD."
  echo
  echo "  ${NEED_BIN}"
  echo "  does not contain: \"${NEED_STR}\""
  echo "  which comes from ${NEED_WHAT}."
  echo
  echo "  Rebuild and INSTALL it, then run this again. Check by hand with:"
  echo "    strings ${NEED_BIN} | grep \"${NEED_STR}\""
  echo
  echo "  Four rounds have been lost to a stale binary already, so this"
  echo "  stops here rather than spending six minutes to find out."
  exit 1
fi
echo "the installed binary is this round's build"
echo

if ! cloonix cli ${NET} dump commands >/dev/null 2>&1; then
  echo "no answer from a cloonix net called '${NET}'."
  echo "  start one:   cloonix start ${NET}"
  exit 1
fi
if ! cloonix cli ${PEER} dump commands >/dev/null 2>&1; then
  echo "no answer from the peer net '${PEER}'. A c2c needs both."
  echo "  start it:    cloonix start ${PEER}"
  exit 1
fi

echo "checking the running binaries carry this round trace"
ROOTFS=""
for cand in frr_trixie trixie wg_trixie mqtt_trixie proxy_trixie alpine; do
  if cloonix --wait 6 cli ${NET} add cnt trprobe eth=2 ${cand} >/dev/null 2>&1; then
    ROOTFS=${cand}
    break
  fi
done
if [ -z "${ROOTFS}" ]; then
  echo "  no usable container image found. Build one first:"
  echo "cd vm_create/cnt_kvm && ./doitall.sh --cnt trixie"
  exit 1
fi
cloonix --wait 2 cli ${NET} plug trprobe eth0 trprobelan >/dev/null 2>&1
sleep 2
if [ -r /var/log/syslog ]; then
  READER="cat /var/log/syslog"
elif sudo -n true 2>/dev/null; then
  READER="sudo cat /var/log/syslog"
else
  READER=""
fi
# Check for THIS ROUND'S trace, not a generic one.
if [ -n "${READER}" ]; then
  cloonix --wait 1 cli ${NET} del trprobe >/dev/null 2>&1
  sleep 4
  LOG=$(${READER} 2>/dev/null | tail -n 4000)
  MISSING=""
  echo "${LOG}" | grep -q "CRUNTRACE trprobe CREATE overlay" \
    || MISSING="${MISSING} CREATE-overlay"
  echo "${LOG}" | grep -q "CRUNTRACE trprobe DELETE asked" \
    || MISSING="${MISSING} DELETE-asked"
  echo "${LOG}" | grep -q "CRUNTRACE trprobe step 3" \
    || MISSING="${MISSING} step-3"
  if [ -n "${MISSING}" ]; then
    echo "  THE TRACE THIS ROUND NEEDS IS NOT IN THE RUNNING BINARIES."
    echo "  missing:${MISSING}"
    echo
    echo "  All of these come from suid_power, which is a SEPARATE binary"
    echo "  from the switch: rebuilding one does not rebuild the other."
    echo "  Rebuild and reinstall it, restart the net, and run again:"
    echo
    echo "cd sources/cloonix/cloonsrc/suid_power && make"
    echo "    <install as usual>"
    echo "cloonix stop ${NET} ; cloonix start ${NET}"
    echo
    echo "  Without it this round produces nothing, which has already"
    echo "  happened three times."
    exit 1
  fi
  echo "  yes, this round's trace is live in suid_power"
else
  echo "  cannot read the syslog from this account, carrying on anyway."
  echo "  The end of this script will tell you the command to run."
fi
cloonix --wait 1 cli ${NET} del trprobe >/dev/null 2>&1
sleep 2
echo "  using the ${ROOTFS} image"
echo

# The run.
echo "running the soak for up to ${MINUTES} minutes, containers and c2c"
echo "(it stops early if the server dies, which is what this is for)"
echo "----------------------------------------------------------"
RUN_START=$(date "+%Y-%m-%d %H:%M:%S")
SOAK_ONLY=cnt,c2c SOAK_PEER=${PEER} "${HERE}/soak.py" ${NET} 4 \
    > ${OUT}/soak.txt 2>&1 &
SOAK=$!
sleep $((MINUTES * 60))
kill -INT ${SOAK} 2>/dev/null
for i in $(seq 1 180); do
  kill -0 ${SOAK} 2>/dev/null || break
  sleep 1
done
kill -0 ${SOAK} 2>/dev/null && kill -INT ${SOAK} 2>/dev/null
wait ${SOAK} 2>/dev/null
RUN_END=$(date "+%Y-%m-%d %H:%M:%S")
cat ${OUT}/soak.txt
echo

# The verdict.
echo "=========================================================="
echo "WHAT SUID_POWER DID FOR THE CONTAINERS THAT LEFT MOUNTS"
echo "=========================================================="

LEFT=$(grep -oE "cnt/sk[a-z][0-9]+/" ${OUT}/soak.txt \
       | sed 's#cnt/##; s#/##' | sort -u)

if [ -z "${LEFT}" ]; then
  echo
  echo "NO MOUNTS WERE LEFT THIS TIME."
  echo
  echo "  Worth one more at double the length before believing it:"
  echo "./test_round.sh ${NET} $((MINUTES * 2))"
  exit 0
fi

echo
echo "containers whose mounts were left: $(echo ${LEFT} | wc -w)"
echo

if [ -z "${READER}" ]; then
  echo "  the syslog cannot be read from this account. Run this and paste it:"
  echo
  for c in ${LEFT}; do
    echo "sudo grep -E 'CRUNTRACE ${c} ' /var/log/syslog"
  done
  exit 0
fi

${READER} 2>/dev/null \
  | awk -v a="${RUN_START}" -v b="${RUN_END}" \
        '{ ts = substr($0, 1, 19); gsub("T", " ", ts) } ts >= a && ts <= b' \
  > ${OUT}/window

# The whole story per container, create and delete together and in order.
echo "  everything that happened to each one, in order:"
echo
for c in ${LEFT}; do
  echo "    --- ${c}"
  grep -oE "CRUNTRACE ${c} [^\"]*" ${OUT}/window \
    | sed "s/CRUNTRACE ${c} /        /" | head -12
  after=$(grep -oE "CRUNTRACE ${c} [^\"]*" ${OUT}/window \
          | grep -n "CREATE overlay" | cut -d: -f1)
  freed=$(grep -oE "CRUNTRACE ${c} [^\"]*" ${OUT}/window \
          | grep -n "step 3: done" | cut -d: -f1)
  if [ -n "${after}" ] && [ -n "${freed}" ] 2>/dev/null; then
    if [ "${after}" -gt "${freed}" ] 2>/dev/null; then
      echo "        ^^ THE OVERLAY WAS MOUNTED AFTER THE DELETE FINISHED."
      echo "           The create was still in flight when the container"
      echo "           was torn down, and nothing will ever unmount this."
    fi
  fi
  echo
done

echo "  every trace line in the window, by kind:"
grep -oE "CRUNTRACE [a-zA-Z0-9]+ [^\"]{0,42}" ${OUT}/window \
  | sed 's/sk[a-z][0-9]*/skX/; s/trprobe/skX/' \
  | sort | uniq -c | sort -rn | head -12 | sed 's/^/    /'
echo

mounted=$(grep -c "CREATE overlay" ${OUT}/window)
finished=$(grep -c "CREATE start" ${OUT}/window)
if [ "${mounted}" -gt "${finished}" ] 2>/dev/null; then
  echo "    $((mounted - finished)) create(s) mounted an overlay and never"
  echo "    reached the last step of the create. Nothing unmounts those:"
  echo "    an aborted create leaves its overlay behind, and this number"
  echo "    should be compared with the count of leftover mounts above --"
  echo "    if they match, that is where the mounts come from."
  echo
fi

asked=$(grep -c "DELETE asked" ${OUT}/window)
started=$(grep -c "step 1:" ${OUT}/window)
if [ "${started}" -gt "${asked}" ] 2>/dev/null && [ "${asked}" -gt 0 ]; then
  echo "    ${started} teardowns began but only ${asked} came through the"
  echo "    delete this trace covers. The other $((started - asked)) enter the"
  echo "    finalisation chain somewhere else, which is worth knowing before"
  echo "    trusting any conclusion about how deletes behave."
  echo
fi
echo "  creates and deletes across the run:"
echo "    CREATE overlay (mounts made) : $(grep -c "CREATE overlay" ${OUT}/window)"
echo "    CREATE start   (create done) : $(grep -c "CREATE start" ${OUT}/window)"
echo "    DELETE asked                 : $(grep -c "DELETE asked" ${OUT}/window)"
echo
echo "  and what the mount check itself reported:"
grep "step 3: .* mount(s) left" ${OUT}/window | sed 's/.*CRUNTRACE /    /' \
  | tail -8
echo

echo "=========================================================="
echo "everything is in ${OUT}/  (soak.txt, window)"
echo "paste the whole of this output"
echo "=========================================================="
