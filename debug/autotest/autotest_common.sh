# autotest_common.sh - sourced by slow_autotest.sh and fast_autotest.sh: two
# kvm of nemo (bundle in kvm1, toy in kvm2), a runner as "user" in each, logs
# in /home/user; no nat, the autotap lan for the host browser: kvm1 10.9.9.1,
# kvm2 10.9.9.2, autotap 10.9.9.3. IMAGES and DEMO_IMAGES set first.
NET="nemo"
. "${HERE}/kvm_autotest_common.sh"
DISTROS="trixie alpine arch fedora44 resolute rocky tumbleweed"
ROOT=$(cd "${HERE}/../.." && pwd)
KVM1="kvm1"
KVM2="kvm2"
RAM="4000"
CPU="4"
TAP="autotap"
LAN="autolan"
WEBNET="10.9.9"
# Modules for the demos and wifi tests, loaded here as root (a user container
# cannot); tun also gives /dev/net/tun to crun (openvpn, tinc).
DEMO_MODULES="tun batman-adv openvswitch geneve wireguard nf_tables af_key xfrm_user esp4 vxlan mac80211_hwsim"
DEMO_PRESENT=""
TMP=$(mktemp -d /tmp/autotest_XXXX)
T_START=$(date +%s)

ssh_vm() { cloonix cli ${NET} ssh "$@"; }
scp_vm() { cloonix cli ${NET} scp "$@"; }

wait_ssh()
{
  local vm="$1" tries=0
  while ! ssh_vm ${vm} "echo" >/dev/null 2>&1; do
    tries=$((tries+1))
    [ ${tries} -ge 120 ] && { echo "FATAL: ${vm} never answered ssh"; exit 1; }
    sleep 3
  done
  echo "${vm} answers"
  # Refused before anything is pushed: the machine lacks a tool of the suites.
  if ! ssh_vm ${vm} "rc=0; for t in ${TOOLS}; do command -v \$t >/dev/null 2>&1 || { echo MISSING \$t; rc=1; }; done; exit \$rc"; then
    echo "FATAL: ${vm} lacks a tool the suites need: ./kvm_autotest_${distro}.sh must install it"
    exit 1
  fi
  if ! ssh_vm ${vm} "test -c /dev/kvm"; then
    echo "WARNING: no /dev/kvm in ${vm}: nested kvm is off on this host,"
    echo "         the kvm checks of the suites will fail (user book, Nested guests)"
  fi
}

# eth0 of both kvm and the tap on one lan, no route (no internet); also
# FIREWALL_OFF for an older machine. A failure warns, the suites run.
web_lan()
{
  local tries=0 n=1 vm
  cloonix cli ${NET} add tap ${TAP} || { echo "WARNING: add tap ${TAP} failed"; return; }
  while ! { cloonix cli ${NET} dump json | grep -q "\"${TAP}\"" && \
            [ -e /sys/class/net/${TAP} ]; }; do
    tries=$((tries+1))
    [ ${tries} -ge 30 ] && { echo "WARNING: no ${TAP}, the web of the kvm is not reachable"; return; }
    sleep 1
  done
  cloonix cli ${NET} plug ${TAP} ${LAN} || echo "WARNING: ${TAP} is not on ${LAN}"
  { sudo ip addr add ${WEBNET}.3/24 dev ${TAP} && sudo ip link set ${TAP} up; } || \
    echo "WARNING: ${TAP} has no ${WEBNET}.3: sudo ip addr add ${WEBNET}.3/24 dev ${TAP}; sudo ip link set ${TAP} up"
  for vm in ${KVM1} ${KVM2}; do
    cloonix cli ${NET} plug ${vm} eth0 ${LAN} || echo "WARNING: ${vm} is not on ${LAN}"
    ssh_vm ${vm} "${FIREWALL_OFF}" || echo "WARNING: ${vm} firewall not checked"
    ssh_vm ${vm} "ip addr add ${WEBNET}.${n}/24 dev eth0 && ip link set eth0 up" || \
      echo "WARNING: ${vm} eth0 has no ${WEBNET}.${n}"
    tries=0
    while ! ping -c1 -W1 ${WEBNET}.${n} >/dev/null 2>&1; do
      tries=$((tries+1))
      if [ ${tries} -ge 10 ]; then
        echo "WARNING: ${vm} does not answer the ping at ${WEBNET}.${n}, its eth0 and rules:"
        ssh_vm ${vm} "ip addr show eth0; nft list ruleset 2>/dev/null | head -40" || true
        break
      fi
    done
    n=$((n+1))
  done
}

push_sources()
{
  local vm="$1"
  scp_vm ${TMP}/sources.tar.gz ${vm}:/home/user
  ssh_vm ${vm} "rm -rf /home/user/sources && mkdir -p /home/user/sources && \
                tar xzf /home/user/sources.tar.gz -C /home/user/sources && \
                rm -f /home/user/sources.tar.gz && \
                chown -R user:user /home/user/sources"
}

# A container image goes as <image>.tar.gz, the form the server extracts at
# the first add.
push_images()
{
  local vm="$1" img
  for img in ${IMAGES}; do
    if [ "${img%.qcow2}" != "${img}" ]; then
      scp_vm ${BULK}/${img} ${vm}:${BULK}
    else
      scp_vm ${TMP}/${img}.tar.gz ${vm}:${BULK}
    fi
    echo "${vm} has ${img} in bulk"
  done
  for img in ${DEMO_PRESENT}; do
    scp_vm ${TMP}/${img}.tar.gz ${vm}:${BULK}
    echo "${vm} has ${img} in bulk (demo image)"
  done
}

# Guest bulk images owned by the user: they run in a user namespace, the
# real user's path and the harder one, the one to test.
own_bulk()
{
  local vm="$1"
  ssh_vm ${vm} "chown -R user:user ${BULK}"
  echo "${vm} bulk owned by user"
}

# The modules the demo images need, loaded in the guest kernel as root
# before the suite runs as user. Best effort: a module the guest kernel does
# not carry warns, and the demo that needs it will skip or say so.
load_modules()
{
  local vm="$1" m
  for m in ${DEMO_MODULES}; do
    # no radio of its own: every radio is a container's
    local args=""
    [ "${m}" = "mac80211_hwsim" ] && args="radios=0"
    if ssh_vm ${vm} "modprobe ${m} ${args} 2>/dev/null"; then
      echo "${vm} loaded ${m}"
    else
      echo "WARNING: ${vm} has no ${m} module, a demo that needs it will skip"
    fi
  done
  ssh_vm ${vm} "test -c /dev/net/tun" || \
    echo "WARNING: ${vm} has no /dev/net/tun, openvpn and tinc demos will skip"
}

install_bundle()
{
  local vm="$1"
  scp_vm ${HOME}/${BUNDLE}.tar.gz ${vm}:/root
  ssh_vm ${vm} "cd /root && tar xzf ${BUNDLE}.tar.gz && cd ${BUNDLE} && ./install_cloonfs"
}

install_toy()
{
  local vm="$1"
  scp_vm ${HOME}/${TOY} ${vm}:/home/user
  ssh_vm ${vm} "chown user:user /home/user/${TOY} && chmod 644 /home/user/${TOY}"
}

# The runner of a kvm: runner_write (suite), runner_endurance (slow only),
# runner_start. Logs in /home/user:
#   results_outputs.log   the suite, a summary, its KERR lines at the end
#   kerr1_outputs.log     those KERR lines alone
#   tst_max_output.log    the endurance, after a suite that passed
#   kerr2_outputs.log     its KERR lines, syslog emptied in between
# kvm1 runs the bundle, kvm2 the toy.
runner_write()
{
  local vm="$1" suite="$2" cmd="$3" expect="$4" prep toylogs=""
  prep=$(( $(date +%s) - T_START ))
  [ "${vm}" = "${KVM2}" ] && toylogs="/home/user/testitalltoy/nemo/rootfs/cloonix/cloonfs/var/log/syslog \
         /home/user/testitalltoy/mito/rootfs/cloonix/cloonfs/var/log/syslog"
  cat > ${TMP}/autotest_run.sh << EOF
#!/bin/bash
# autotest_run.sh - written by debug/autotest: in ${vm} as user, ${suite}:
# ${cmd}
# The spare interfaces of the outside vm, for the phy sub test: eth1 for
# macvlan, eth2 for absorb. eth0 is the lifeline and never named here.
export TESTITALL_PHY=eth1
export TESTITALL_PHY_ABSORB=eth2
LOG=/home/user/results_outputs.log
KERR1=/home/user/kerr1_outputs.log
SOAK=/home/user/tst_max_output.log
KERR2=/home/user/kerr2_outputs.log
SYSLOG=/var/log/syslog
# The toy's daemons log inside the rootfs of each toy; only its proxymous
# reaches /var/log/syslog.
TOYLOGS="${toylogs}"
rm -f \${LOG} \${KERR1} \${SOAK} \${KERR2}

# The KERR lines of the syslogs as they come, tail -F picking the toy's up
# when they appear; kerr_stop ends the tail, the grep follows it.
kerr_follow() { tail -n0 -F \${SYSLOG} \${TOYLOGS} 2>/dev/null | grep --line-buffered -E "KERR|CLOONIX-PHASE" > \$1 & }
kerr_stop() { pkill -f "tail -n0 -F \${SYSLOG}"; }

summary()
{
  local rc=\$1 t=\$2 nb_ok nb_skip nb_toyno nb_fail nb_kerr verdict d
  nb_ok=\$(grep -v "^      \[" \${LOG} | grep -c "ok in")
  nb_skip=\$(grep "^  SKIP " \${LOG} | grep -c " no image: ")
  nb_toyno=\$(grep "^  SKIP " \${LOG} | grep -c " toy: not possible")
  nb_fail=\$(grep -c "^FAILED:\\|^  FAIL  " \${LOG})
  nb_kerr=\$(wc -l < \${KERR1})
  d=\$(( \$(date +%s) - t ))
  if [ \${rc} -eq 0 ]; then verdict="PASSED"; else verdict="FAILED (exit \${rc})"; fi
  echo
  echo "=========================================================="
  echo "SUMMARY ${suite} in ${vm}: \${verdict}"
  echo "=========================================================="
  echo "  duration:           \${d}s"
  echo "  preparation:        ${prep}s (host side, before the suite)"
  echo "  total:              \$(( d + ${prep} ))s"
  echo "  phases reached:"
  grep -E "^[0-9]+\\.[0-9]* " \${LOG} | sed "s/^/    /"
  echo "  checks ok:          \${nb_ok}"
  echo "  demos skipped:      \${nb_skip} (image not in the bulk of this kvm, see DEMO_IMAGES)"
  [ \${nb_toyno} -eq 0 ] || echo "  demos not for a toy: \${nb_toyno} (TOY_NO of demo_test.py)"
  grep "^  SKIP " \${LOG} | awk '!seen[\$0]++' | sed "s/^/  /"
  echo "  failed:             \${nb_fail}"
  grep -E "^FAILED:|^  FAIL  " \${LOG} | sed "s/^/    /"
  echo "  KERR lines of syslog during the suite: \${nb_kerr} (folded and listed at the end of this file)"
  if grep -q -E "^  MAX " \${LOG}; then
    echo "  maximum flows (testbigflows.py, udp and frames at 1% loss at most):"
    grep -E "^  MAX " \${LOG} | awk '!seen[\$0]++' | sed "s/^/  /"
  fi
  echo "=========================================================="
  if [ \${rc} -ne 0 ]; then
    echo
    for i in 1 2 3 4 5; do
      echo "FAILED FAILED FAILED  ${suite} in ${vm} exited \${rc}  FAILED FAILED FAILED"
    done
    echo
  fi
}

grep -E "^Sig(Ign|Blk)" /proc/self/status > \${LOG}
sudo -n truncate -s 0 \${SYSLOG}
# The syslog path is proven before the KERR counts are trusted.
python3 -c 'import syslog; syslog.syslog("autotest-probe")'; sleep 2
grep -q autotest-probe \${SYSLOG} || \
  { echo "WARNING: nothing reaches \${SYSLOG}, the KERR logs will be empty"; \
    systemctl status rsyslog syslog-ng@default 2>&1 | grep -E "Active|Loaded" ; } >> \${LOG}
kerr_follow \${KERR1}
t0=\$(date +%s)
cd /home/user/sources/debug
( ${cmd} ) 2>&1 | python3 -u progress.py ${expect} >> \${LOG}
rc=\${PIPESTATUS[0]}
sleep 2
kerr_stop
summary \${rc} \${t0} >> \${LOG}
# Fold the whole KERR log into the one results file, so it is complete on its
# own and a single file is enough to hand over: the output, the summary, then
# every KERR line the suite produced.
{ echo; python3 /home/user/sources/debug/kerr_triage.py \${KERR1} 2>&1; \\
  echo; echo "=========================================================="; \\
  echo "all KERR lines of syslog during the suite (\$(wc -l < \${KERR1}) lines)"; \\
  echo "=========================================================="; \\
  cat \${KERR1} 2>/dev/null; } >> \${LOG}
if [ \${rc} -ne 0 ]; then
  { echo "--- the machine ---"; ls -ld /var/lib/cloonix /var/lib/cloonix/bulk; ls -l /var/lib/cloonix/bulk; \\
    ls -l /var/lib/cloonix/*/dtach/ 2>/dev/null; \\
    sudo -n sysctl kernel.io_uring_disabled 2>&1; lsmod | grep -E "^(kvm|openvswitch|vhost|tun)"; \\
    free -m; grep -E "^(pswpin|pswpout|pgmajfault|allocstall|oom_kill)" /proc/vmstat; } >> \${LOG} 2>&1
  { echo "--- dmesg ---"; sudo -n dmesg | tail -20; \\
    echo "--- dmesg: every oom kill ---"; \\
    sudo -n dmesg | grep -E "invoked oom-killer|oom-kill:|Killed process|oom_reaper" | tail -20; \\
    echo "--- dmesg: stalls, lockups, full tables ---"; \\
    sudo -n dmesg | grep -i -E "stall|lockup|hung|blocked for|clocksource|backlog|table full|throttl" | tail -20; \\
    echo "--- pressure, file systems ---"; \\
    grep -H . /proc/pressure/cpu /proc/pressure/io /proc/pressure/memory 2>/dev/null; \\
    df -T /tmp /home /var/log ${BULK}; uname -r; nproc; \\
    sudo -n cat /sys/kernel/debug/sched/features 2>&1 | head -3; } >> \${LOG} 2>&1
  for f in /var/lib/cloonix/*/dtach/*.stderr \\
           /home/user/testitalltoy/*/rootfs/var/lib/cloonix/*/dtach/*.stderr \\
           /home/user/testitalltoy/*/worktmp/crun.log \\
           /home/user/testitalltoy/*/rootfs/root/cloonix-init-starter-crun.log; do
    [ -f "\${f}" ] && { echo "--- \${f} ---"; tail -30 "\${f}"; } >> \${LOG}
  done
  echo >> \${LOG}
  echo "RESULTS COMPLETE: ${suite} in ${vm} FAILED. This file is final." >> \${LOG}
  exit \${rc}
fi
echo >> \${LOG}
echo "RESULTS COMPLETE: ${suite} in ${vm} PASSED. This file is final." >> \${LOG}
EOF
}

# After a suite that passed, the endurance with no end, on a syslog emptied
# of the suite, sized for the RAM of the kvm; its KERR lines are a
# population of their own and are folded on their own.
runner_endurance()
{
  local soak="$1"
  cat >> ${TMP}/autotest_run.sh << EOF
echo "The endurance (${soak}) now runs with no end into \${SOAK}, its KERR in \${KERR2}." >> \${LOG}
export SOAK_MAX_LIVE=20 SOAK_MAX_LIVE_KVM=2
sudo -n truncate -s 0 \${SYSLOG}
kerr_follow \${KERR2}
./${soak} > \${SOAK} 2>&1
echo "${soak} exit \$?" >> \${SOAK}
kerr_stop
{ echo; python3 /home/user/sources/debug/kerr_triage.py \${KERR2} 2>&1; } >> \${SOAK}
EOF
}

# su gives no session: the two variables name the bus of the lingering user@
# (crun cgroups); exec'd through python and setsid -f to reset signals.
runner_start()
{
  local vm="$1"
  scp_vm ${TMP}/autotest_run.sh ${vm}:/home/user
  ssh_vm ${vm} "chown user:user /home/user/autotest_run.sh; chmod 755 /home/user/autotest_run.sh"
  cat > ${TMP}/autotest_launch.py << EOF
import os, signal
for s in (signal.SIGINT, signal.SIGQUIT, signal.SIGTERM, signal.SIGPIPE,
          signal.SIGCHLD):
    signal.signal(s, signal.SIG_DFL)
signal.pthread_sigmask(signal.SIG_SETMASK, [])
os.execv("/home/user/autotest_run.sh", ["/home/user/autotest_run.sh"])
EOF
  scp_vm ${TMP}/autotest_launch.py ${vm}:/home/user
  ssh_vm ${vm} "test -S /run/user/\$(id -u user)/bus || echo 'WARNING: no user bus for user in ${vm}'"
  ssh_vm ${vm} "su - user -c 'XDG_RUNTIME_DIR=/run/user/\$(id -u) \
                DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/\$(id -u)/bus \
                CLOONIX_TOY_CGROUPM=${CLOONIX_TOY_CGROUPM} \
                setsid -f nohup python3 /home/user/autotest_launch.py \
                > /home/user/autotest_run.err 2>&1 </dev/null'"
}

# Everything before the runners: the distro checked, the images packed, nemo
# restarted, the two kvm booted on autolan and filled with the sources, the
# images, the modules, the bundle in kvm1 and the toy in kvm2.
autotest_prepare()
{
  local image img vm
  distro="${1:-trixie}"
  set -e
  trap 'rm -rf ${TMP}' EXIT
  # kvm_autotest_<distro>, .sh or .qcow2 name the same machine.
  distro="${distro#kvm_autotest_}"; distro="${distro%.sh}"; distro="${distro%.qcow2}"
  case " ${DISTROS} " in
    *" ${distro} "*) : ;;
    *) echo "FATAL: '${1}' is not a distro of the autotest, one of: ${DISTROS}"; exit 1 ;;
  esac
  image="kvm_autotest_${distro}.qcow2"
  if [ ! -f "${BULK}/${image}" ]; then
    echo "FATAL: ${BULK}/${image} is missing: ./kvm_autotest_${distro}.sh"
    exit 1
  fi
  for img in ${IMAGES}; do
    if [ ! -e "${BULK}/${img}" ] && [ ! -f "${BULK}/${img}.tar.gz" ]; then
      echo "FATAL: ${BULK}/${img} is missing, the suites need it"
      echo "       vm_create/cnt_kvm and vm_create/cnt_kvm_demo build them"
      exit 1
    fi
  done
  BUNDLE=$(ls -t ${HOME}/cloonix-bundle.tar.gz 2>/dev/null | head -1)
  TOY=$(ls -t ${HOME}/cloonix-toy.tar.xz 2>/dev/null | head -1)
  [ -n "${BUNDLE}" ] || { echo "FATAL: no cloonix-bundle.tar.gz in ${HOME}"; exit 1; }
  [ -n "${TOY}" ] || { echo "FATAL: no cloonix-toy.tar.xz in ${HOME}"; exit 1; }
  BUNDLE=$(basename ${BUNDLE} .tar.gz)
  TOY=$(basename ${TOY})
  for f in /sys/module/kvm_intel/parameters/nested /sys/module/kvm_amd/parameters/nested; do
    if [ -f ${f} ] && ! grep -qE '^(Y|1)$' ${f}; then
      echo "WARNING: nested kvm is off (${f}), see the user book, Nested guests"
    fi
  done
  echo "======================================================================"
  echo " $(basename $0): ${image}"
  echo "   ${KVM1}  ${BUNDLE}.tar.gz"
  echo "   ${KVM2}  ${TOY}"
  echo "======================================================================"

  echo "--- packing the sources and the images ---"
  tar czf ${TMP}/sources.tar.gz --exclude=.git --exclude=rootfs_targz_store \
      --exclude=work_targz_store -C ${ROOT} .
  for img in ${IMAGES}; do
    [ "${img%.qcow2}" != "${img}" ] && continue
    if [ ! -d "${BULK}/${img}" ]; then
      cp -f ${BULK}/${img}.tar.gz ${TMP}/${img}.tar.gz
      continue
    fi
    # A directory is packed as cnt_common_pack does: its content, owners as
    # numbers; built with --map-auto its files belong to subuids, so from
    # the same map.
    if ! unshare --map-root-user --map-auto -- \
           tar --numeric-owner -czf ${TMP}/${img}.tar.gz -C ${BULK}/${img} . 2>/dev/null && \
       ! sudo -n tar --numeric-owner -czf ${TMP}/${img}.tar.gz -C ${BULK}/${img} .; then
      echo "FATAL: cannot pack ${BULK}/${img} whole, neither with a subuid map nor with sudo"
      exit 1
    fi
  done
  # The demo images that are present, packed the same way; a missing one is
  # skipped with a warning and its demo will skip in the guest.
  for img in ${DEMO_IMAGES}; do
    if [ -d "${BULK}/${img}" ]; then
      if ! unshare --map-root-user --map-auto -- \
             tar --numeric-owner -czf ${TMP}/${img}.tar.gz -C ${BULK}/${img} . 2>/dev/null && \
         ! sudo -n tar --numeric-owner -czf ${TMP}/${img}.tar.gz -C ${BULK}/${img} .; then
        echo "WARNING: cannot pack ${BULK}/${img}, its demo will skip in the guest"
        continue
      fi
    elif [ -f "${BULK}/${img}.tar.gz" ]; then
      cp -f ${BULK}/${img}.tar.gz ${TMP}/${img}.tar.gz
    else
      echo "WARNING: ${BULK}/${img} is not built, its demo will skip in the guest"
      continue
    fi
    DEMO_PRESENT="${DEMO_PRESENT} ${img}"
  done

  net_restart
  set -e
  # Three interfaces each: eth0 stays the lifeline to autolan/autotap, eth1
  # and eth2 are spare so the guest's cloonix can macvlan and absorb them
  # (the phy sub test) without touching the connection.
  for vm in ${KVM1} ${KVM2}; do
    cloonix cli ${NET} add kvm ${vm} ram=${RAM} cpu=${CPU} eth=3 ${image} &
  done
  sleep 5
  wait_ssh ${KVM1}
  wait_ssh ${KVM2}
  web_lan

  for vm in ${KVM1} ${KVM2}; do
    echo "--- preparing ${vm} ---"
    push_sources ${vm}
    push_images ${vm}
    own_bulk ${vm}
    load_modules ${vm}
  done
  install_bundle ${KVM1}
  install_toy ${KVM2}
}

# The last words on the host: where to look.
autotest_banner()
{
  echo
  echo "======================================================================"
  echo " the suite is done, and results_outputs.log final, at its line 'RESULTS COMPLETE'"
  echo " (PASSED or FAILED); its summary gives the duration and the total with the"
  echo " preparation ($(( $(date +%s) - T_START ))s here)"
  echo " the web, user nemo password nemoclown (4454 mito mitoclown):"
  echo "   https://${WEBNET}.1:4444 ${KVM1}    https://${WEBNET}.2:4444 ${KVM2}"
  # At column 0: pasted with a leading space, bash (ignorespace) keeps no history of it.
  echo "cloonix cli ${NET} ssh ${KVM1} \"tail -f /home/user/results_outputs.log\""
  echo "cloonix cli ${NET} ssh ${KVM2} \"tail -f /home/user/results_outputs.log\""
}
