#!/bin/bash
# teststorm.sh - flows on a moving target: iperf3 clients that never stop
# while the topology around and under them is torn and rebuilt at random.
#   ./teststorm.sh
# Needs the image flow_trixie (vm_create/cnt_kvm_demo/cnt_flow_trixie.sh).
# Cleans and starts nemo and mito, then runs teststorm.py on them; the
# first thing that does not hold ends it, the nets left as they are.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "${HERE}"
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
./teststorm.py nemo mito
