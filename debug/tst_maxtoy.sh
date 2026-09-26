#!/bin/bash
# tst_max.sh on the toys: testitalltoy.sh makes and starts them, its env
# points soak.py at their client, work zones and syslogs. Every step is
# checked.
if ! ./testitalltoy.sh start; then
  echo
  echo "the toys did not start, so nothing below can be trusted."
  exit 1
fi
eval $(./testitalltoy.sh env)

# The sizes are those of the development host; the environment overrides
# them (the autotest gives its 4 GB kvm smaller ones).
SOAK_PEER=mito SOAK_AGENT=1 SOAK_MAX_LIVE=${SOAK_MAX_LIVE:-45} SOAK_C2C=2 SOAK_SNF_KILL=300 \
SOAK_WEIGHTS=kvm=20 SOAK_MAX_LIVE_KVM=${SOAK_MAX_LIVE_KVM:-25} \
./soak.py nemo 8 2>&1 | tee /tmp/eternal_toy.txt
