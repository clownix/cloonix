#!/bin/bash
# top_testitall.sh - the whole non-regression, short phases first, the first
# failure ends it.
#   ./top_testitall.sh
#   ./top_testitall.sh start      phase 1 only, the nets up (top_testfast.sh calls it)
# 1 nemo and mito  2 doc checks  3 testitall.py  4 sub_*.py  5 demo_test.py
# 6 testfailures.py  7 testbigflows.py (10 min storm)  8 teststorm.py (30 min)
# 9 tst_max.sh, the endurance (/tmp/eternal.txt)
# The start, with the date of the installed build, and each phase are
# CLOONIX-PHASE lines in the syslog: keep them when the KERR are taken out.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "${HERE}"
# On any failure (set -e or an explicit exit) end with a FAIL line five times
# over, so a glance at the tail of the log gives the verdict at once.
fail_banner() {
  local rc=$?
  trap - EXIT
  [ "${rc}" -eq 0 ] && exit 0
  echo
  for i in 1 2 3 4 5; do
    echo "FAILED FAILED FAILED  top_testitall.sh exited ${rc}  FAILED FAILED FAILED"
  done
  echo
  exit "${rc}"
}
trap fail_banner EXIT
export TESTBIGFLOWS_LONG=${TESTBIGFLOWS_LONG:-600}
export TESTSTORM_SECS=${TESTSTORM_SECS:-1800}
# passt says every connection event in its log beside its socket: a reset on
# a fresh publish is being chased, publish_evidence of testitall.py prints it.
export CLOONIX_PASST_DEBUG=1

# one line of the machine at every phase: how it fares over the night
machine_line()
{
  echo "      machine: $(awk '/MemAvailable/{printf "%d MB free", $2/1024}' /proc/meminfo)," \
       "load $(cut -d" " -f1-3 /proc/loadavg)," \
       "bulk $(df -h --output=avail /var/lib/cloonix/bulk 2>/dev/null | tail -1 | tr -d ' ') left," \
       "/tmp $(df -h --output=avail /tmp 2>/dev/null | tail -1 | tr -d ' ') left"
}

# A CLOONIX-PHASE line in the syslog, like those of testitall.py and
# demo_test.py: every KERR after it belongs to that phase.
mark()
{
  logger -t top_testitall.sh "CLOONIX-PHASE $*" 2>/dev/null || true
}

phase()
{
  echo
  echo "=========================================================="
  echo "$1"
  echo "=========================================================="
  machine_line
  mark "top_testitall.sh $1"
}

# The start of this launch and the build it tests: a syslog kept across
# launches is read from the last such line.
if [ "$1" != "start" ]; then
  BUILT=$(stat -c %y /usr/libexec/cloonix/cloonfs/bin/cloonix-main-server \
          2>/dev/null | cut -d. -f1)
  LAUNCH="top_testitall.sh start $(date '+%F %T'), cloonix-main-server of ${BUILT:-unknown date}"
  mark "${LAUNCH}"
  echo "this launch in the syslog: from the line CLOONIX-PHASE ${LAUNCH}"
  # what the run leaves in /tmp is this launch's only
  rm -rf /tmp/testbigflows_max_nemo.txt /tmp/eternal.txt /tmp/soak_*.txt \
         /tmp/demo_log
fi

phase "1. the nets"
# The soak of an earlier launch would file what the clean below removes as
# faults: containers seen dying, a server gone.
if pkill -KILL -f "soak\.py nemo " 2>/dev/null; then
  echo "the soak of an earlier launch was still running on nemo: stopped"
fi
# fido too: demo_test starts it for c2c.py, one left running would keep the
# binaries of an earlier install
for net in nemo mito fido; do
  ./fail_spider.sh clean ${net}
done
for net in nemo mito; do
  cloonix start ${net}
done
for net in nemo mito; do
  ok=0
  for w in $(seq 1 30); do
    if cloonix cli ${net} dump commands >/dev/null 2>&1; then
      ok=1
      break
    fi
    sleep 1
  done
  if [ ${ok} -eq 0 ]; then
    echo "${net} started but never answered a cli in 30 seconds."
    exit 1
  fi
done
[ "$1" = "start" ] && exit 0

# TESTITALL_SUBS_FIRST (space list of sub_*.py) and TESTITALL_DEMOS_FIRST
# (comma list of demos) run those first, so any test still under development
# fails in minutes, not after the long phases; the full suite follows.
if [ -n "${TESTITALL_SUBS_FIRST}${TESTITALL_DEMOS_FIRST}" ]; then
  phase "1.1 priority tests first"
  for psub in ${TESTITALL_SUBS_FIRST}; do
    echo "--- ${psub} ---"
    python3 ./${psub} nemo mito
  done
  if [ -n "${TESTITALL_DEMOS_FIRST}" ]; then
    python3 ./demo_test.py nemo --only "${TESTITALL_DEMOS_FIRST}"
  fi
fi

phase "2. the docs and the tree"
./check_docs.sh
./check_doc_words.sh
python3 ./check_cli.py
python3 ./check_doc_cli.py
python3 ./check_site.py
./check_wire.sh
python3 ./check_kerr.py
python3 ./check_demos.py --quiet
if command -v sphinx-build >/dev/null 2>&1; then
  for book in xdoc_user xdoc_dev; do
    out=$(mktemp -d /tmp/testitall_${book}_XXXX)
    if sphinx-build -q -W -b html ../httpdoc/${book} ${out} \
         >${out}.log 2>&1; then
      echo "${book} builds with no warning"
    else
      echo "${book} does not build clean:"
      cat ${out}.log
      exit 1
    fi
    rm -rf ${out} ${out}.log
  done
else
  echo "(no sphinx-build: the two books are not built)"
fi

phase "3. every item, on nemo with mito as the peer"
python3 ./testitall.py nemo mito

phase "4. one feature at a time: the sub tests"
for sub in ./sub_*.py; do
  echo
  python3 ${sub} nemo mito
done

phase "5. every demo"
python3 ./demo_test.py nemo mito

phase "6. things going wrong on purpose"
python3 ./testfailures.py nemo mito

phase "7. every flow path above its limit"
python3 ./testbigflows.py nemo mito

phase "8. flows on a moving target"
python3 ./teststorm.py nemo mito

echo
echo "=========================================================="
echo "maximum flows of the bundle"
echo "=========================================================="
# Written by testbigflows.py (phase 7): the maximum of every device.
cat /tmp/testbigflows_max_nemo.txt 2>/dev/null || echo "(no maximum flows: testbigflows.py did not finish)"

echo
echo "TESTITALL OK"
# The autotest runs the endurance itself, into logs of its own.
if [ -n "${TESTITALL_NO_ENDURANCE}" ]; then
  exit 0
fi
phase "9. the endurance: tst_max.sh"
exec ./tst_max.sh
