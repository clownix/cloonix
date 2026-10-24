#!/bin/bash
# teststorm.sh - endless iperf3 flows while the topology is torn and rebuilt
# at random (image flow_trixie); starts nemo and mito, first failure ends it.
#   ./teststorm.sh
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
