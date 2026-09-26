#!/bin/bash
# testitall.sh - the whole non-regression in one command, first failure ends it.
#   ./testitall.sh
# The short phases first, the long ones last: a failure is known at once.
# 1. clean and start nemo and mito
# 2. the doc checks: names, words, cli spelling, wire, demos, sphinx -W
# 3. testitall.py: every item of cloonix, one check each, sniffed
# 4. sub_*.py: one feature each, in the order ls gives, each runnable alone
# 5. demo_test.py: every demo of demos/, on nemo and mito side by side, the
#    long ones last, the net empty and the syslog read after each one
# 6. testfailures.py: bad input, killed helpers, aborted clients, garbage
#    on the ports, guests that misbehave, a chowned image, a net killed
# 7. testbigflows.py: every flow path driven above its limit, the storm of
#    all of them at once held 10 minutes
# 8. teststorm.py: flows on a moving target, adds, dels, sniffs and unsniffs
#    thrown at random for 30 minutes under iperf3 clients that never stop
# 9. tst_max.sh: the endurance, all night, /tmp/eternal.txt
# The stress phases run long here on purpose; the .sh of each one keeps its
# short default for a run by hand.
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
    echo "FAILED FAILED FAILED  testitall.sh exited ${rc}  FAILED FAILED FAILED"
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
rm -f /tmp/testbigflows_max_nemo.txt

# one line of the machine at every phase: how it fares over the night
machine_line()
{
  echo "      machine: $(awk '/MemAvailable/{printf "%d MB free", $2/1024}' /proc/meminfo)," \
       "load $(cut -d" " -f1-3 /proc/loadavg)," \
       "bulk $(df -h --output=avail /var/lib/cloonix/bulk 2>/dev/null | tail -1 | tr -d ' ') left," \
       "/tmp $(df -h --output=avail /tmp 2>/dev/null | tail -1 | tr -d ' ') left"
}

echo "=========================================================="
echo "1. the nets"
echo "=========================================================="
machine_line
for net in nemo mito; do
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

# Debug shortcut. TESTITALL_SUBS_FIRST (space list of sub_*.py) and
# TESTITALL_DEMOS_FIRST (comma list of demos) run those first, so any test
# still under development fails in minutes, not after the long phases. With
# TESTITALL_QUICK also set, the suite stops right after them: a fast, complete
# pass/fail of just the risky set. All unset: nothing here, full suite in order.
if [ -n "${TESTITALL_SUBS_FIRST}${TESTITALL_DEMOS_FIRST}" ]; then
  echo
  echo "=========================================================="
  echo "1.1 priority tests first"
  echo "=========================================================="
  for psub in ${TESTITALL_SUBS_FIRST}; do
    echo "--- ${psub} ---"
    python3 ./${psub} nemo mito
  done
  if [ -n "${TESTITALL_DEMOS_FIRST}" ]; then
    python3 ./demo_test.py nemo --only "${TESTITALL_DEMOS_FIRST}"
  fi
  if [ -n "${TESTITALL_QUICK}" ]; then
    echo
    echo "=========================================================="
    echo "PRIORITY TESTS ALL PASSED -- stopping before the full suite"
    echo "(TESTITALL_QUICK set; unset it to run everything)"
    echo "=========================================================="
    exit 0
  fi
fi

echo
echo "=========================================================="
echo "2. the docs and the tree"
echo "=========================================================="
machine_line
./check_docs.sh
./check_doc_words.sh
python3 ./check_cli.py
python3 ./check_doc_cli.py
python3 ./check_site.py
./check_wire.sh
python3 ./check_demos.py --quiet
if command -v sphinx-build >/dev/null 2>&1; then
  for book in xdoc_user xdoc_dev; do
    out=$(mktemp -d /tmp/testitall_${book}_XXXX)
    # as httpdoc/cmd builds it: index.rst is not staged there
    if sphinx-build -q -W -b html -D exclude_patterns=index.rst \
         ../httpdoc/${book} ${out} >${out}.log 2>&1; then
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

echo
echo "=========================================================="
echo "3. every item, on nemo with mito as the peer"
echo "=========================================================="
machine_line
python3 ./testitall.py nemo mito

echo
echo "=========================================================="
echo "4. one feature at a time: the sub tests"
echo "=========================================================="
machine_line
for sub in ./sub_*.py; do
  echo
  python3 ${sub} nemo mito
done

echo
echo "=========================================================="
echo "5. every demo"
echo "=========================================================="
machine_line
python3 ./demo_test.py nemo mito

echo
echo "=========================================================="
echo "6. things going wrong on purpose"
echo "=========================================================="
machine_line
python3 ./testfailures.py nemo mito

echo
echo "=========================================================="
echo "7. every flow path above its limit"
echo "=========================================================="
machine_line
python3 ./testbigflows.py nemo mito

echo
echo "=========================================================="
echo "8. flows on a moving target"
echo "=========================================================="
machine_line
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
echo
echo "=========================================================="
echo "9. the endurance: tst_max.sh"
echo "=========================================================="
machine_line
exec ./tst_max.sh
