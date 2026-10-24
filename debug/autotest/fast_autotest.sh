#!/bin/bash
# fast_autotest.sh [<distro>] - top_testfast.sh on the bundle (kvm1) and the
# toy (kvm2), about 10 minutes, no endurance; ends with the step times, the
# longest first. Only the images top_testfast.sh names.
#   ./fast_autotest.sh                kvm_autotest_trixie.qcow2
#   ./fast_autotest.sh rocky          kvm_autotest_rocky.qcow2
HERE=$(cd "$(dirname "$0")" && pwd)
. "${HERE}/autotest_common.sh"
IMAGES=${AUTOTEST_IMAGES:-"trixie.qcow2 trixie perf_trixie"}
DEMO_IMAGES=""

autotest_prepare "$@"
runner_write ${KVM1} top_testfast.sh "./top_testfast.sh" 900
runner_start ${KVM1}
runner_write ${KVM2} "top_testfast.sh (toy)" \
  "./top_testitalltoy.sh start && ./top_testitalltoy.sh run ./top_testfast.sh" 900
runner_start ${KVM2}
autotest_banner
echo "======================================================================"
