#!/bin/bash
# testfailures.sh - faults on purpose (bad input, killed helpers, aborted
# clients, port garbage, bad guests, chowned image, net killed); starts nemo
# and mito, the first unhandled fault ends it.
#   ./testfailures.sh
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
./testfailures.py nemo mito
