#!/bin/bash
# testitalltoy.sh - testitall on two toys, nemo and mito, from the tar.gz.
#   ./testitalltoy.sh [cloonix-toy-<num>.tar.gz]     the whole run, then stop
#   ./testitalltoy.sh start [cloonix-toy-<num>.tar.gz]  make and start the toys only
#   ./testitalltoy.sh run <command...>                one test on the running toys:
#                                                     ./testitalltoy.sh run ./testfailures.py nemo mito
#   ./testitalltoy.sh env                             the exports, for: eval $(./testitalltoy.sh env)
#   ./testitalltoy.sh stop                            stop the toys
# No tarball: the newest ${HOME}/cloonix-toy-*.tar.gz, unpacked twice under
# ${TESTITALLTOY_DIR}/nemo and /mito. Kills any nemo/mito (bundle or toy), runs
# phases 2 to 8 of testitall.sh with the toy's client (CLOONIX_CLIENT), config
# (TESTITALL_CFG) and demos (CLOONIX_DEMO_DIR); TESTITALL_TOY marks the checks
# that are not the toy's. Not run here: the endurance (tst_maxtoy.sh).
# /dev/kvm, /dev/vhost-net and /dev/net/tun must be readable and writable.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "${HERE}"

VERB="all"
case "$1" in
  start|run|env|stop) VERB=$1; shift ;;
esac
TOYDIR=${TESTITALLTOY_DIR:-${HOME}/testitalltoy}
export TESTBIGFLOWS_LONG=${TESTBIGFLOWS_LONG:-600}
export TESTSTORM_SECS=${TESTSTORM_SECS:-1800}
NETS="nemo mito"

toys_env() {
  export TESTITALL_TOY="${TOYDIR}"
  # setup.sh wrote which user namespace the toy got: with the single mapping
  # setgroups is denied inside and the daemons that drop privileges die at
  # start, so the demos running them are skipped, with the reason.
  IDMAP=$(head -c 6 "${TOYDIR}/nemo/config/idmap" 2>/dev/null)
  if [ "${IDMAP}" != "subuid" ]; then
    export TESTITALL_TOY_SINGLE_IDMAP=1
    echo "the toy runs with the single uid mapping: setgroups is denied inside," >&2
    echo "the demos of daemons that drop privileges are skipped (see setup.sh above)" >&2
  fi
  export CLOONIX_CLIENT="${TOYDIR}/nemo/bin/cloonix"
  export TESTITALL_CFG="${TOYDIR}/nemo/rootfs/cloonix/cloonfs/etc/cloonix.cfg"
  export CLOONIX_DEMO_DIR="${TOYDIR}/nemo/rootfs/cloonix/cloonfs/share/cloonix/demos"
  export TESTITALL_SYSLOG="${TOYDIR}/nemo/rootfs/cloonix/cloonfs/var/log/syslog"
  CLI="python3 ${CLOONIX_CLIENT}"
}

stop_toys() {
  echo
  echo "=========================================================="
  echo "the two toys stopped"
  echo "=========================================================="
  for net in ${NETS}; do
    (cd "${TOYDIR}/${net}" && ./${net}cmd stop) || true
  done
}

# Stop the toys, and on a failure end with a FAIL line five times over, so the
# tail of the log gives the verdict at a glance.
on_exit() {
  local rc=$?
  trap - EXIT
  stop_toys
  if [ "${rc}" -ne 0 ]; then
    echo
    for i in 1 2 3 4 5; do
      echo "FAILED FAILED FAILED  testitalltoy.sh exited ${rc}  FAILED FAILED FAILED"
    done
    echo
  fi
  exit "${rc}"
}

case "${VERB}" in
  env)
    toys_env
    for v in TESTITALL_TOY CLOONIX_CLIENT TESTITALL_CFG CLOONIX_DEMO_DIR TESTITALL_SYSLOG TESTITALL_TOY_SINGLE_IDMAP; do
      [ -n "${!v}" ] && echo "export ${v}=${!v}"
    done
    exit 0 ;;
  run)
    if [ ! -x "${TOYDIR}/nemo/nemocmd" ]; then
      echo "no toys in ${TOYDIR}: ./testitalltoy.sh start first"
      exit 1
    fi
    toys_env
    exec "$@" ;;
  stop)
    stop_toys
    exit 0 ;;
esac

TARGZ=${1:-$(ls -t ${HOME}/cloonix-toy-*.tar.gz 2>/dev/null | head -1)}
if [ -z "${TARGZ}" ] || [ ! -f "${TARGZ}" ]; then
  echo "no cloonix-toy-<num>.tar.gz in ${HOME} and none given"
  exit 1
fi

# one line of the machine at every phase: how it fares over the night
machine_line()
{
  echo "      machine: $(awk '/MemAvailable/{printf "%d MB free", $2/1024}' /proc/meminfo)," \
       "load $(cut -d" " -f1-3 /proc/loadavg)," \
       "bulk $(df -h --output=avail /var/lib/cloonix/bulk 2>/dev/null | tail -1 | tr -d ' ') left," \
       "/tmp $(df -h --output=avail /tmp 2>/dev/null | tail -1 | tr -d ' ') left"
}

echo "=========================================================="
echo "1. the toys, from ${TARGZ}"
echo "=========================================================="
machine_line
# A bundle net answers the installed client, a previous toy has its cmd, the
# rest is found by process name: the main server and the bundle's proxymous
# carry the net last, the toy's proxymous and container init have it in their name.
net_procs() {
  pgrep -f "cloonix-main-server .* $1\$|cloonix-proxymous .* $1\$|cloonix-proxymous-$1|cloonix-init-${USER}-$1" 2>/dev/null || true
}
for net in ${NETS}; do
  if command -v cloonix >/dev/null 2>&1; then
    cloonix stop ${net} >/dev/null 2>&1 || true
  fi
  if [ -x "${TOYDIR}/${net}/${net}cmd" ]; then
    "${TOYDIR}/${net}/${net}cmd" stop >/dev/null 2>&1 || true
  fi
done
# A kill unwinds the net: guests, mounts, then the server and its proxymous
# go, and the bundle's share directory with them. Wait for that, and only
# then take what is left by force.
for net in ${NETS}; do
  for w in $(seq 1 40); do
    if [ -z "$(net_procs ${net})" ] && [ ! -d "/tmp/cloonix_proxymous_${net}" ]; then
      break
    fi
    sleep 1
  done
  left=$(net_procs ${net})
  if [ -n "${left}" ]; then
    echo "${net}: still there 40s after the kill, killing: $(echo ${left})"
    kill -9 ${left} 2>/dev/null || true
    sleep 2
  fi
  if [ -n "$(net_procs ${net})" ]; then
    echo "a process of ${net} will not die: $(ps -o pid,args -p $(net_procs ${net} | tr '\n' ',' | sed 's/,$//'))"
    exit 1
  fi
  rm -rf "/tmp/cloonix_proxymous_${net}" "/tmp/${net}_proxymous_${USER}"
done

# With the subuid mapping, files an inner container wrote in the toy belong
# to subuids: only a namespace of the same map can erase them.
if [ -d "${TOYDIR}" ]; then
  unshare --map-root-user --map-auto rm -rf "${TOYDIR}" 2>/dev/null || rm -rf "${TOYDIR}"
  if [ -e "${TOYDIR}" ]; then
    echo "${TOYDIR} of a previous run cannot be erased: unshare --map-root-user --map-auto rm -rf ${TOYDIR}"
    exit 1
  fi
fi
mkdir -p "${TOYDIR}"
for net in ${NETS}; do
  mkdir -p "${TOYDIR}/${net}"
  tar xzf "${TARGZ}" -C "${TOYDIR}/${net}"
  top=$(ls -d "${TOYDIR}/${net}"/cloonix-toy-* | head -1)
  if [ -z "${top}" ]; then
    echo "${TARGZ} holds no cloonix-toy-<num> directory"
    exit 1
  fi
  # the toy is used from its own directory: setup.sh reads pwd
  mv "${top}"/* "${top}"/.[!.]* "${TOYDIR}/${net}/" 2>/dev/null || true
  rmdir "${top}"
  (cd "${TOYDIR}/${net}" && ./setup.sh ${net})
  if [ ! -x "${TOYDIR}/${net}/${net}cmd" ]; then
    echo "setup.sh ${net} made no ${net}cmd in ${TOYDIR}/${net}"
    exit 1
  fi
done

for net in ${NETS}; do
  (cd "${TOYDIR}/${net}" && ./${net}cmd start)
done

toys_env
for net in ${NETS}; do
  ok=0
  for w in $(seq 1 30); do
    if ${CLI} cli ${net} dump commands >/dev/null 2>&1; then
      ok=1
      break
    fi
    sleep 1
  done
  if [ ${ok} -eq 0 ]; then
    echo "the toy ${net} started but never answered a cli in 30 seconds."
    exit 1
  fi
done

# A kernel can starve what runs outside a busy cgroup: where each side is.
echo "cgroups, proxymous beside its toy (kernel $(uname -r)):"
for p in $(pgrep -f "cloonix-proxymous-|cloonix-main-server"); do
  echo "  $(tr '\0' ' ' < /proc/${p}/cmdline | cut -c1-70)  $(cat /proc/${p}/cgroup)"
done

if [ "${VERB}" = "start" ]; then
  echo
  echo "the two toys run; tests on them:  ./testitalltoy.sh run ./testfailures.py nemo mito"
  echo "or in this shell:                  eval \$(./testitalltoy.sh env)"
  echo "when done:                         ./testitalltoy.sh stop"
  exit 0
fi
trap on_exit EXIT
rm -f /tmp/testbigflows_max_nemo.txt
# Debug shortcut. TESTITALL_SUBS_FIRST (space list of sub_*.py) and
# TESTITALL_DEMOS_FIRST (comma list of demos) run those first; with
# TESTITALL_QUICK also set, the toy suite stops right after them. All unset:
# nothing here.
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
echo "maximum flows of the toy"
echo "=========================================================="
# Written by testbigflows.py (phase 7): the maximum of every device.
cat /tmp/testbigflows_max_nemo.txt 2>/dev/null || echo "(no maximum flows: testbigflows.py did not finish)"

echo
echo "TESTITALLTOY OK"
