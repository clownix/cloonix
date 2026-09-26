#!/bin/bash
# Every step is checked.
for net in nemo mito; do
  if ! ./fail_spider.sh clean ${net}; then
    echo
    echo "the clean of ${net} failed, so nothing below it can be trusted."
    echo "fix that first: the reason is in the lines above."
    exit 1
  fi
done

for net in nemo mito; do
  if ! cloonix start ${net}; then
    echo
    echo "cloonix start ${net} failed to start."
    exit 1
  fi
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

# The sizes are those of the development host; the environment overrides
# them (the autotest gives its 4 GB kvm smaller ones).
SOAK_PEER=mito SOAK_AGENT=1 SOAK_MAX_LIVE=${SOAK_MAX_LIVE:-45} SOAK_C2C=2 SOAK_SNF_KILL=300 \
SOAK_WEIGHTS=kvm=20 SOAK_MAX_LIVE_KVM=${SOAK_MAX_LIVE_KVM:-25} \
./soak.py nemo 8 2>&1 | tee /tmp/eternal.txt
