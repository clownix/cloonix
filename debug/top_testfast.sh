#!/bin/bash
# top_testfast.sh - fast non-regression: every item once, short sub tests,
# two demos, first failure ends it, step times printed. Starts nemo and mito,
# except under top_testitalltoy.sh run (TESTITALL_TOY).
#   ./top_testfast.sh
#   ./top_testitalltoy.sh start && ./top_testitalltoy.sh run ./top_testfast.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
cd "${HERE}"
SUBS="sub_rpc.py sub_web_refusals.py sub_web_ws_topo.py sub_client_exits.py \
sub_position_readback.py sub_item_note.py sub_ftopo_consistency.py sub_snf_corner.py \
sub_sniff_modes.py sub_a2b_replace.py sub_c2c_replug.py sub_c2c_mac_mangle.py \
sub_c2c_udp_garbage.py sub_twonets_samename.py sub_web_ctl.py sub_cntwifi.py \
sub_fail_publish_busy.py sub_fail_bad_images.py sub_userns_container.py \
sub_container_devices.py sub_phy.py sub_host_clean.py"
DEMOS="cloonix/cnt.py,cloonix/cntwifi.py"
T0=$(date +%s)
TIMES=""
CUR=""
CUR_T=0

step()
{
  CUR="$1"; shift
  CUR_T=$(date +%s)
  "$@"
  TIMES="${TIMES}$(( $(date +%s) - CUR_T )) ${CUR}
"
  CUR=""
}

# The table, then on a failure the FAIL line five times over.
on_exit()
{
  local rc=$?
  trap - EXIT
  [ -n "${CUR}" ] && TIMES="${TIMES}$(( $(date +%s) - CUR_T )) ${CUR} FAILED
"
  echo
  echo "=========================================================="
  echo "duration of every step, the longest first"
  echo "=========================================================="
  printf "%s" "${TIMES}" | sort -rn | awk '{t=$1; $1=""; printf "  %5ds %s\n", t, $0}'
  echo "  total $(( $(date +%s) - T0 ))s"
  if [ "${rc}" -ne 0 ]; then
    echo
    for i in 1 2 3 4 5; do
      echo "FAILED FAILED FAILED  top_testfast.sh exited ${rc}  FAILED FAILED FAILED"
    done
    echo
  fi
  exit "${rc}"
}
trap on_exit EXIT

echo "=========================================================="
echo "1. the nets"
echo "=========================================================="
if [ -n "${TESTITALL_TOY}" ]; then
  echo "the toys of top_testitalltoy.sh, started already"
else
  step "nemo and mito started" ./top_testitall.sh start
fi

echo
echo "=========================================================="
echo "2. every item, on nemo with mito as the peer"
echo "=========================================================="
step testitall.py python3 ./testitall.py nemo mito

echo
echo "=========================================================="
echo "3. the short sub tests"
echo "=========================================================="
for sub in ${SUBS}; do
  echo
  step ${sub} python3 ./${sub} nemo mito
done

echo
echo "=========================================================="
echo "4. two demos"
echo "=========================================================="
step demos python3 ./demo_test.py nemo mito --only ${DEMOS}

echo
echo "TESTFAST OK"
