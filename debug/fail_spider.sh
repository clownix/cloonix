#!/bin/bash
# fail_spider.sh - create a lot of containers as fast as possible, on purpose,
# and find out exactly which one goes missing.

if [ "$1" = "clean" ]; then
  NET=${2:-nemo}
  DIR=/var/lib/cloonix/${NET}
  echo "cleaning ${DIR}"

  why_alive() {
    local pid=$1 st
    st=$(ps -o stat= -p ${pid} 2>/dev/null | tr -d " ")
    case "${st}" in
      "")   echo "gone" ;;
      Z*)   echo "state ${st} (zombie: already exited, waiting on its parent \
$(ps -o ppid= -p ${pid} 2>/dev/null | tr -d ' '))" ;;
      D*)   echo "state ${st} (uninterruptible, stuck in the kernel: a mount \
or an i/o that never returns. kill cannot touch it)" ;;
      *)    echo "state ${st}" ;;
    esac
  }

  running=$(pgrep -f "cloonix-main-server.* ${NET}$" 2>/dev/null | head -1)
  if [ -n "${running}" ]; then
    echo "  the net is still running (cloonix-main-server pid ${running})"
    echo "  asking it to stop"
    cloonix stop ${NET} >/dev/null 2>&1
    # 40, not 15.
    for w in $(seq 1 40); do
      pgrep -f "cloonix-main-server.* ${NET}$" >/dev/null 2>&1 || break
      sleep 1
    done
    if pgrep -f "cloonix-main-server.* ${NET}$" >/dev/null 2>&1; then
      echo "  it did not stop. Killing the servers by hand:"
      for pat in cloonix-main-server cloonix-gui-server cloonix-doorways \
                 cloonix-proxymous cloonix-ovs-drv cloonix-suid-power \
                 cloonix-ovsdb-server cloonix-ovs-vswitchd; do
        for pid in $(pgrep -f "${pat}.*${NET}" 2>/dev/null); do
          echo -n "    ${pat} pid ${pid}"
          err=$(sudo kill -9 ${pid} 2>&1)
          if [ -n "${err}" ]; then
            echo " -- kill said: ${err}"
          else
            echo
          fi
        done
      done
      sleep 2
    fi

    still=$(pgrep -f "cloonix-main-server.* ${NET}$" 2>/dev/null)
    if [ -n "${still}" ]; then
      echo "  THE NET IS STILL UP. cloonix-main-server survived kill -9:"
      for pid in ${still}; do
        echo "    pid ${pid}: $(why_alive ${pid})"
        echo "      cmdline: $(tr "\0" " " < /proc/${pid}/cmdline 2>/dev/null)"
        echo "      wchan  : $(cat /proc/${pid}/wchan 2>/dev/null)"
      done
      echo
      echo "  Nothing below this point can work while it runs: it holds the"
      echo "  mounts of ${DIR} in its own namespace, so rm -rf fails and a"
      echo "  new net cannot have the same name. Stopping here rather than"
      echo "  cleaning around it."
      exit 1
    fi
    echo "  the net is down, carrying on with the cleanup"
  fi

  NB=$(ls /tmp/config.json.sk* /tmp/config.json.spd* 2>/dev/null | wc -l)
  if [ "${NB}" != "0" ]; then
    echo "  removing ${NB} stray /tmp/config.json.* left by older builds"
    rm -f /tmp/config.json.sk* /tmp/config.json.spd* 2>/dev/null
  fi

  if command -v crun >/dev/null 2>&1; then
    for c in $(sudo crun --root=${DIR}/crun list 2>/dev/null | awk 'NR>1{print $1}'); do
      echo "  killing container ${c}"
      sudo crun --root=${DIR}/crun kill ${c} KILL >/dev/null 2>&1
      sudo crun --root=${DIR}/crun delete --force ${c} >/dev/null 2>&1
    done
  fi
  for n in $(ip netns list 2>/dev/null | awk '{print $1}' | grep "${NET}"); do
    echo "  deleting netns ${n}"
    sudo ip netns del ${n} >/dev/null 2>&1
  done

  # Leaked helper processes: a passt per nat, an ovs-a2b per a2b.
  for pat in "cloonix-passt" "cloonix-ovs-a2b" "cloonix-ovs-snf"; do
    for pid in $(pgrep -f "${pat}.*${NET}" 2>/dev/null); do
      echo "  killing leaked ${pat} pid ${pid}"
      sudo kill -9 ${pid} 2>/dev/null
    done
  done

  # Deepest first: .../tmpfs/upper has to be unmounted before .../tmpfs, and a
  # reverse sort of the paths puts them in that order.
  n=0
  for m in $(mount | awk '{print $3}' | grep "^${DIR}" | sort -r); do
    if sudo umount "${m}" 2>/dev/null || sudo umount -l "${m}" 2>/dev/null; then
      n=$((n+1))
    else
      echo "  COULD NOT UNMOUNT ${m}"
    fi
  done
  echo "  unmounted ${n}"

  # "mount" shows THIS namespace only.
  elsewhere=$(sudo awk -v d="${DIR}" \
              '$5 == d || index($5, d "/") == 1 { print $5 }' \
              /proc/*/mountinfo 2>/dev/null | sort -u)
  left=$(mount | awk '{print $3}' | grep -c "^${DIR}")
  if [ "${left}" -ne 0 ] || [ -n "${elsewhere}" ]; then
    echo "  still mounted, not removing anything. What holds them:"
    for m in $(mount | awk '{print $3}' | grep "^${DIR}"; echo "${elsewhere}"); do
      [ -z "${m}" ] && continue
      echo "    ${m}"
      sudo fuser -vm "${m}" 2>&1 | sed "s/^/      /"
    done
    echo
    echo "  A mount this script cannot see in its own namespace belongs to a"
    echo "  process that still has one: a crun, or a server that survived."
    exit 1
  fi
  if sudo rm -rf ${DIR}; then
    echo "  ${DIR} removed"
  else
    echo "  COULD NOT REMOVE ${DIR}. What is left, and what holds it:"
    sudo ls -la ${DIR} 2>&1 | head -n 20 | sed "s/^/    /"
    sudo fuser -vm ${DIR} 2>&1 | sed "s/^/    /"
    exit 1
  fi
  exit 0
fi

NET=${1:-nemo}
COUNT=${2:-60}
ROOTFS=${3:-frr_trixie}
ACK=${4:-0.05}
ROUNDS=${5:-1}
WAIT_READY=${6:-0}

PREFIX=spd
OUT=/tmp/fail_spider

rm -rf ${OUT}
mkdir -p ${OUT}

# Is the net there at all?
if ! cloonix cli ${NET} dump commands >/dev/null 2>&1; then
  echo "no answer from a cloonix net called '${NET}'"
  echo "  start one first:   cloonix start ${NET}"
  exit 1
fi

echo "fail_spider: ${COUNT} containers on ${NET}, image ${ROOTFS},"
echo "             ack-wait ${ACK}s, ${ROUNDS} round(s), no retries, no checking"
echo

round=0
while [ ${round} -lt ${ROUNDS} ]; do
round=$((round+1))
[ ${ROUNDS} -gt 1 ] && echo "================ round ${round} of ${ROUNDS}"

echo -n "clearing anything left from before"
for i in $(seq 1 ${COUNT}); do
  cloonix --wait 0.05 cli ${NET} del ${PREFIX}${i} >/dev/null 2>&1
done
for w in $(seq 1 30); do
  left=0
  for i in $(seq 1 ${COUNT}); do
    if cloonix --wait 2 cli ${NET} ssh ${PREFIX}${i} "echo x" >/dev/null 2>&1; then
      left=$((left+1))
    fi
  done
  [ ${left} -eq 0 ] && break
  echo -n "."
  sleep 1
done
sleep 2
echo " done"
echo

# The burst.
START=$(date "+%Y-%m-%d %H:%M:%S")
START_EPOCH=$(date "+%s")
echo "started at ${START}"
echo -n "creating: "

refused=0
for i in $(seq 1 ${COUNT}); do
  name=${PREFIX}${i}
  t0=$(date "+%s.%N")
  out=$(cloonix --wait ${ACK} cli ${NET} add cnt ${name} eth=2 \
        ${ROOTFS} 2>&1)
  rc=$?
  t1=$(date "+%s.%N")
  el=$(echo "${t1} ${t0}" | awk '{printf "%.3f", $1 - $2}')
  echo "${name} rc=${rc} secs=${el} out=${out}" >> ${OUT}/creates
  if [ ${rc} -ne 0 ]; then
    refused=$((refused+1))
    echo -n "x"
  else
    echo -n "."
  fi
done
if [ "${WAIT_READY}" = "1" ]; then
  echo
  echo -n "waiting for all ${COUNT} to answer before cabling"
  wait_start=$(date "+%s")
  for w in $(seq 1 120); do
    up=0
    for i in $(seq 1 ${COUNT}); do
      cloonix --wait 2 cli ${NET} ssh ${PREFIX}${i} "echo x" >/dev/null 2>&1 \
        && up=$((up+1))
    done
    [ ${up} -eq ${COUNT} ] && break
    echo -n "."
  done
  echo " ${up}/${COUNT} up after $(($(date "+%s") - wait_start))s"
fi

echo
echo -n "cabling:  "
lan_refused=0
for i in $(seq 1 $((COUNT-1))); do
  j=$((i+1))
  for pair in "${PREFIX}${i} 1" "${PREFIX}${j} 0"; do
    set -- ${pair}
    out=$(cloonix --wait ${ACK} cli ${NET} plug $1 eth$2 flan${i} 2>&1)
    rc=$?
    echo "lan flan${i} $1 eth$2 rc=${rc} out=${out}" >> ${OUT}/lans
    [ ${rc} -ne 0 ] && lan_refused=$((lan_refused+1))
  done
  echo -n "."
done
echo

END=$(date "+%Y-%m-%d %H:%M:%S")
echo
echo "finished at ${END}, $(($(date "+%s") - START_EPOCH))s for ${COUNT}"\
" creates and $((COUNT-1)) lans"
echo

sleep 3
cloonix cli ${NET} dump commands > ${OUT}/lst 2>&1

missing=""
present=0
for i in $(seq 1 ${COUNT}); do
  name=${PREFIX}${i}
  if grep -q " ${name} " ${OUT}/lst; then
    present=$((present+1))
  else
    missing="${missing} ${name}"
  fi
done

missing_lan=""
lans_present=0
for i in $(seq 1 $((COUNT-1))); do
  n=$(grep -c "flan${i}\b" ${OUT}/lst)
  if [ "${n}" -ge 2 ]; then
    lans_present=$((lans_present+1))
  else
    missing_lan="${missing_lan} flan${i}(${n}/2)"
  fi
done

immediate=${lans_present}
settle_max=90
settled=${lans_present}
if [ ${lans_present} -lt $((COUNT-1)) ]; then
  echo -n "waiting for the cabling to settle"
  for w in $(seq 1 ${settle_max}); do
    sleep 2
    cloonix cli ${NET} dump commands > ${OUT}/lst 2>&1
    settled=0
    for i in $(seq 1 $((COUNT-1))); do
      n=$(grep -c "flan${i}\b" ${OUT}/lst)
      [ "${n}" -ge 2 ] && settled=$((settled+1))
    done
    [ ${settled} -ge $((COUNT-1)) ] && break
    echo -n "."
  done
  echo " ${settled}/$((COUNT-1)) after $((w*2))s"
  # recount the missing ones against the settled picture
  missing_lan=""
  lans_present=${settled}
  for i in $(seq 1 $((COUNT-1))); do
    n=$(grep -c "flan${i}\b" ${OUT}/lst)
    [ "${n}" -ge 2 ] || missing_lan="${missing_lan} flan${i}(${n}/2)"
  done
  missing=""
  present=0
  for i in $(seq 1 ${COUNT}); do
    if grep -q " ${PREFIX}${i} " ${OUT}/lst; then
      present=$((present+1))
    else
      missing="${missing} ${PREFIX}${i}"
    fi
  done
  echo
fi

echo "containers: asked for ${COUNT}, the server has ${present}"
echo "lans      : asked for $((COUNT-1)), the server has ${lans_present}"
if [ "${immediate}" != "${lans_present}" ]; then
  echo "            (${immediate} of them straight away, the rest arrived"
  echo "             while the containers finished booting -- they were"
  echo "             pending, not lost)"
fi
echo "refused at the client (non-zero exit): ${refused} creates,"\
" ${lan_refused} lans"
echo

if [ -z "${missing}" ] && [ -z "${missing_lan}" ]; then
  if [ ${round} -lt ${ROUNDS} ]; then
    echo "round ${round}: nothing missing, going again"
    echo
    continue
  fi
  echo "NOTHING MISSING in ${round} round(s)."
  echo
  echo "  Worth trying before concluding it is fixed. The bug needs both a"
  echo "  lot of them and no gap between them, so push whichever is lower:"
  echo "./fail_spider.sh ${NET} $((COUNT*2)) ${ROOTFS} ${ACK}"
  # LC_ALL=C: awk formats %f in the locale (0,0100 in French).
  FASTER=$(LC_ALL=C awk -v a="${ACK}" 'BEGIN { printf "%.4f", a / 5 }')
  echo "./fail_spider.sh ${NET} ${COUNT} ${ROOTFS} ${FASTER}"
  echo
  echo "  And the other direction is the useful control: if the same count"
  echo "  with a two second ack never loses one, the rate is the trigger"
  echo "  rather than the number."
  echo "./fail_spider.sh ${NET} ${COUNT} ${ROOTFS} 2"
  echo
  echo "  An intermittent fault found never is not evidence of absence, so"
  echo "  the fifth argument runs it over and over and stops at the first"
  echo "  round that loses something:"
  echo "./fail_spider.sh ${NET} ${COUNT} ${ROOTFS} ${ACK} 20"
  exit 0
fi

[ -n "${missing}" ]     && echo "MISSING CONTAINERS:${missing}"
[ -n "${missing_lan}" ] && echo "MISSING LANS:${missing_lan}"
echo
if [ -z "${missing}" ]; then
  echo "  Every container is there and it is the CABLING that was lost."
  echo "  That is a different fault from a container going missing, and a"
  echo "  more likely one: the lans were asked for while all ${COUNT} were"
  echo "  still booting, which is the busiest the switch ever gets."
  echo
  lo=$(echo ${missing_lan} | tr " " "\n" | sed "s/flan//;s/(.*//" \
       | sort -n | head -1)
  hi=$(echo ${missing_lan} | tr " " "\n" | sed "s/flan//;s/(.*//" \
       | sort -n | tail -1)
  nlost=$(echo ${missing_lan} | wc -w)
  echo "  ${nlost} of $((COUNT-1)) lans lost, numbered ${lo} to ${hi}."
  half=$(((COUNT-1)/2))
  if [ "${lo}" -gt "${half}" ]; then
    echo
    echo "  EVERY ONE IS IN THE SECOND HALF. Containers are created in"
    echo "  number order, so the high numbers are the ones still booting"
    echo "  when the cabling began. That is the shape of cabling an"
    echo "  interface that does not exist yet, not of a switch under load."
    echo
    echo "  The experiment that settles it -- wait for all of them to"
    echo "  answer, then cable:"
    echo "./fail_spider.sh ${NET} ${COUNT} ${ROOTFS} ${ACK} 1 1"
    echo "  If nothing is lost that way, the cause is the demos cabling"
    echo "  too early, and the fix belongs there."
  fi
  echo
fi
if [ -n "${missing}" ]; then
echo "what the client saw for each missing container, at the time:"
for name in ${missing}; do
  grep "^${name} " ${OUT}/creates | sed "s/^/    /"
done
fi
if [ -n "${missing_lan}" ]; then
echo "what the client saw for each missing lan:"
for l in ${missing_lan}; do
  grep "^lan ${l%%(*} " ${OUT}/lans | sed "s/^/    /"
done
fi
echo
echo "  rc=1 means it was refused and the reason is in the out= field."
echo
echo "  rc=0 does NOT mean it was accepted."
echo
echo "  It means the client heard nothing within ${ACK}s and stopped"
echo "  waiting. A refusal arriving after that is never seen: the server"
echo "  said no and wrote a KERR, and the client had already moved on to"
echo "  the next command. With an ack this short, rc=0 on every missing"
echo "  one proves nothing about where they went."
echo
echo "  The syslog below decides it. The same run with a two second ack"
echo "  confirms it:"
echo "./fail_spider.sh ${NET} ${COUNT} ${ROOTFS} 2"
echo "  If the losses come back as rc=1 with a reason attached, the server"
echo "  was refusing all along and only the client was deaf."
echo

READER=""
if [ -r /var/log/syslog ]; then
  READER="cat /var/log/syslog"
elif sudo -n true 2>/dev/null; then
  READER="sudo cat /var/log/syslog"
elif journalctl -n 1 >/dev/null 2>&1; then
  READER="journalctl --no-pager --since ${START}"
fi

echo "the switch between ${START} and ${END}:"
if [ -n "${READER}" ]; then
  ${READER} 2>/dev/null \
    | awk -v a="${START}" -v b="${END}" '
        { ts = substr($0, 1, 19); gsub("T", " ", ts) }
        ts >= a && ts <= b' \
    | grep -E "cloonix|KERR" > ${OUT}/syslog_window
  n=$(grep -c . ${OUT}/syslog_window 2>/dev/null)
  if [ "${n}" -gt 0 ] 2>/dev/null; then
    echo "  (${n} lines, read with: ${READER})"
    echo
    echo "  what it complained about, most often first:"
    sed "s/^.*KERR: //" ${OUT}/syslog_window \
      | sed "s/[0-9][0-9]*/N/g" | sort | uniq -c | sort -rn | head -n 12 \
      | sed "s/^/    /"
    echo
    # If LAN_TRACE is on in the switch, this is the line that decides it.
  seen=$(grep -c "LANTRACE.*ARRIVED" ${OUT}/syslog_window 2>/dev/null)
  att=$(grep -c "LANTRACE.*ATTACHING" ${OUT}/syslog_window 2>/dev/null)
  if [ "${seen}" -gt 0 ] 2>/dev/null; then
    echo "  the switch's own count (LAN_TRACE is on):"
    echo "    plug requests that ARRIVED at the switch : ${seen}"
    echo "    of those, reaching the attach              : ${att}"
    echo "    the client sent                            : $((2*(COUNT-1)))"
    echo
    if [ "${seen}" -lt $((2*(COUNT-1))) ]; then
      echo "    FEWER ARRIVED THAN WERE SENT. The requests are being lost"
      echo "    before the switch ever sees them -- in the client, the"
      echo "    doorways connection or proxymous -- and nothing inside"
      echo "    recv_add_lan.c can be the cause."
    else
      echo "    ALL OF THEM ARRIVED. The loss is inside the switch, and"
      echo "    the branch each one took is in the lines below."
    fi
    echo
    echo "  where each request went:"
    grep "LANTRACE" ${OUT}/syslog_window | sed "s/^.*LANTRACE //" \
      | sed "s/[0-9][0-9]*/N/g" | sort | uniq -c | sort -rn | head -n 8 \
      | sed "s/^/    /"
    echo
  fi
  echo "  anything about lans, coherency or a lock:"
    grep -iE "lan|coheren|lock|ovsreq" ${OUT}/syslog_window \
      | tail -n 20 | sed "s/^/    /"
    echo
    echo "  the last 15 lines of the window:"
    tail -n 15 ${OUT}/syslog_window | sed "s/^/    /"
  else
    echo "    NOTHING AT ALL from cloonix in that window."
    echo
    echo "    If LAN_TRACE is 1 in uml_cloonix_switch/src/recv_add_lan.c"
    echo "    there should be one ARRIVED line per plug here. None at"
    echo "    all means the switch running is not the one just built:"
    echo "      grep -n \"define LAN_TRACE\" \\"
    echo "        <sources>/cloonix/cloonsrc/uml_cloonix_switch/src/recv_add_lan.c"
    echo "    then rebuild, reinstall, and restart the net before rerunning."
    echo
    echo "    That is a fact and not a gap: $(echo ${missing_lan} | wc -w)"\
" lans and $(echo ${missing} | wc -w) containers"
    echo "    were lost and the switch wrote not one line about any of"
    echo "    them. Whatever dropped them did so on a path with no KERR."
  fi
else
  echo "    the log cannot be read from this account. Any of these will"
  echo "    do it, and the second is the one that matters:"
  echo
  echo "sudo awk '\$0 >= \"${START}\" && \$0 <= \"${END}\"' \\"
  echo "        /var/log/syslog | grep -E 'cloonix|KERR' | tail -60"
  echo
  echo "sudo grep -E 'recv_add_lan|coheren|LOCK|ovsreq' \\"
  echo "        /var/log/syslog | tail -40"
  echo
  echo "    The second one is the question this whole run exists to ask:"
  echo "    did the switch refuse those $(echo ${missing_lan} | wc -w) lans"\
" and say so, or did they"
  echo "    vanish with nothing written about them at all?"
fi
echo
if [ -n "${missing}" ]; then
echo "and anything naming the missing containers, anywhere in the log:"
for name in ${missing}; do
  if [ -r /var/log/syslog ]; then
    grep -- "${name}" /var/log/syslog 2>/dev/null | tail -n 8 | sed "s/^/    /"
  fi
done
echo
fi

if [ -n "${missing}" ]; then
echo "does the missing one exist anywhere on the machine?"
for name in ${missing}; do
  echo "  ${name}:"
  echo -n "    in the topology (lst): "
  grep -q " ${name} " ${OUT}/lst && echo yes || echo no
  echo -n "    a crun container:      "
  if command -v crun >/dev/null 2>&1; then
    crun --root=/var/lib/cloonix/${NET}/crun list 2>/dev/null \
      | grep -q "${name}" && echo yes || echo no
  else
    echo "(crun not on the path here)"
  fi
  echo -n "    a directory left over: "
  ls -d /var/lib/cloonix/${NET}/cnt/${name} >/dev/null 2>&1 && echo yes || echo no
  echo -n "    a network namespace:   "
  ip netns list 2>/dev/null | grep -q "${name}" && echo yes || echo no
done
echo
fi
echo "everything collected is in ${OUT}/"
echo "  creates        every create, its exit code and how long it took"
echo "  lst            what the server says it has"
echo "  syslog_window  the log between the first and last create"
echo
echo "the net is left up on purpose: the state after the failure is the"
echo "evidence. Clear it with:  cloonix cli ${NET} del all"
exit 1
done
