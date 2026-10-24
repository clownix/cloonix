# Sourced by kvm_<distro>_<desktop>.sh, NET VMNAME SRC QCOW2 BULK set first.
# Nothing fails quietly: fatal removes the image, else a WARNING, repeated at
# the end and counted by doitall.sh.
DESK_WARN=""
DESK_MADE=0

desk_fatal()
{
  echo
  echo "FATAL: $*"
  cloonix stop ${NET} >/dev/null 2>&1
  if [ "${DESK_MADE}" = "1" ]; then
    rm -f ${BULK}/${QCOW2}
    echo "       ${BULK}/${QCOW2} is removed: it is not a usable desktop image."
  fi
  exit 1
}

desk_warn()
{
  echo "WARNING: $*"
  DESK_WARN="${DESK_WARN}
  WARNING: $*"
}

# desk_ssh <what> <command>: the command in the guest, fatal when it fails.
desk_ssh()
{
  local rc
  cloonix cli ${NET} ssh ${VMNAME} "$2"
  rc=$?
  [ "${rc}" = "0" ] || desk_fatal "$1 (exit ${rc})"
}

# desk_opt <what> <command>: the same, a warning when it fails.
desk_opt()
{
  local rc
  cloonix cli ${NET} ssh ${VMNAME} "$2"
  rc=$?
  [ "${rc}" = "0" ] || desk_warn "$1 (exit ${rc})"
}

# desk_install <what> <command> [<second choice>]
desk_install()
{
  local rc
  cloonix cli ${NET} ssh ${VMNAME} "$2"
  rc=$?
  [ "${rc}" = "0" ] && return
  [ -n "$3" ] || desk_fatal "$1 (exit ${rc})"
  desk_warn "$1 (exit ${rc}), second choice taken: $3"
  desk_ssh "$1, second choice" "$3"
}

# A fresh net, the base image copied, the guest up, the mirror reachable and
# the account user there.
desk_start()
{
  local tries
  [ -f ${BULK}/${SRC} ] || \
    desk_fatal "${BULK}/${SRC} is missing, build it first: ./kvm_${SRC%.qcow2}.sh"
  if cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; then
    echo "a server is up for ${NET}, stopping it"
    cloonix stop ${NET}
  fi
  tries=0
  while cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; do
    tries=$((tries+1))
    [ ${tries} -ge 30 ] && desk_fatal "${NET} will not stop"
    echo "waiting for ${NET} to stop"
    sleep 2
  done
  tries=0
  while true; do
    cloonix start ${NET} 2>&1 | grep -v "^$"
    sleep 2
    cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server && break
    tries=$((tries+1))
    [ ${tries} -ge 15 ] && desk_fatal "${NET} will not start"
    echo "server not up yet, retrying"
    sleep 2
  done
  cp -vf ${BULK}/${SRC} ${BULK}/${QCOW2} || \
    desk_fatal "${BULK}/${SRC} could not be copied to ${BULK}/${QCOW2}"
  DESK_MADE=1
  sync
  # A desktop install needs more ram than the base image.
  cloonix cli ${NET} add kvm ${VMNAME} ram=4000 cpu=2 eth=1 ${QCOW2} --persistent || \
    desk_fatal "add kvm ${VMNAME} on ${QCOW2} refused"
  tries=0
  while ! cloonix cli ${NET} ssh ${VMNAME} "echo" >/dev/null 2>&1; do
    tries=$((tries+1))
    [ ${tries} -ge 100 ] && desk_fatal "${VMNAME} does not answer after 5 minutes"
    echo "${VMNAME} not ready, waiting 3 sec"
    sleep 3
  done
  cloonix cli ${NET} add nat nat || desk_fatal "add nat refused"
  cloonix cli ${NET} plug nat lan_nat || desk_fatal "plug nat refused"
  cloonix cli ${NET} plug ${VMNAME} eth0 lan_nat || desk_fatal "plug ${VMNAME} refused"
  # ipv4 only dhcp (dhclient -6 never returns without nat ipv6), bounded; the
  # mirror wait below is the test.
  timeout 120 cloonix cli ${NET} ssh ${VMNAME} "\
    ip link set dev eth0 up; \
    ip -4 addr show dev eth0 | grep -q 'inet ' && exit 0; \
    command -v dhcpcd  >/dev/null 2>&1 && dhcpcd  eth0     && exit 0; \
    command -v udhcpc  >/dev/null 2>&1 && udhcpc -i eth0 -q -n && exit 0; \
    command -v dhclient >/dev/null 2>&1 && { dhclient -4 -1 eth0; exit 0; }; \
    command -v nmcli   >/dev/null 2>&1 && nmcli device connect eth0 && exit 0; \
    exit 0"
  [ "$?" = "124" ] && desk_warn "the dhcp client of ${SRC} did not return in 120s, stopped"
  tries=0
  while ! cloonix cli ${NET} ssh ${VMNAME} "ping -c1 -W2 10.117.0.1 >/dev/null 2>&1"; do
    tries=$((tries+1))
    if [ ${tries} -ge 20 ]; then
      cloonix cli ${NET} ssh ${VMNAME} "ip -4 addr show dev eth0; ip route"
      desk_fatal "the mirror 10.117.0.1 is not reachable from ${VMNAME}, eth0 is as shown above"
    fi
    echo "mirror not reachable yet, waiting 3 sec"
    sleep 3
  done
  desk_user
}

# The account every desktop logs in as, password user. trixie, resolute and
# arch have it from their base image; alpine, rocky, fedora44 and tumbleweed
# do not. useradd where there is one, the adduser of busybox otherwise.
desk_user()
{
  desk_ssh "the account user could not be created" \
    "id user >/dev/null 2>&1 && exit 0; \
     sh=\$(command -v bash || echo /bin/sh); \
     if command -v useradd >/dev/null 2>&1; then useradd -m -s \${sh} user; \
     else adduser -D -s \${sh} user; fi && \
     echo user:user | chpasswd && id user"
}

# A repo section that metalinks or mirrorlists to the internet is turned off,
# section by section: one unreachable repo fails the whole transaction.
desk_rpm_no_internet_repo()
{
  desk_ssh "norepo.awk could not be written in the guest" "cat > /root/norepo.awk << 'AWKEOF'
function flush(  i, l) {
  if (sect == \"\") return
  print sect
  for (i = 1; i <= n; i++) {
    l = line[i]
    if (net && l ~ /^enabled=1/) { l = \"enabled=0\"; changed = 1 }
    print l
  }
}
/^\\[/ { flush(); sect = \$0; n = 0; net = 0; next }
      { if (sect == \"\") { print; next }
        line[++n] = \$0
        if (\$0 ~ /^(mirrorlist|metalink)=/) net = 1 }
END   { flush(); if (changed) print \"CHANGED\" > \"/dev/stderr\" }
AWKEOF"
  desk_ssh "the internet repos could not be turned off" "set -e; for f in /etc/yum.repos.d/*.repo; do \
    awk -f /root/norepo.awk \${f} > \${f}.new 2>/root/norepo.log; \
    mv -f \${f}.new \${f}; \
    if grep -q CHANGED /root/norepo.log; then echo \"turned off an internet repo in \${f}\"; fi; \
  done; rm -f /root/norepo.awk /root/norepo.log"
}

# desk_disable <unit>...: a unit the image does not have is nominal.
desk_disable()
{
  desk_opt "some units could not be disabled, named above" "rc=0; \
    for u in $*; do \
      systemctl cat \${u} >/dev/null 2>&1 || continue; \
      systemctl disable \${u} || { echo \"could not disable \${u}\"; rc=1; }; \
    done; exit \${rc}"
}

# desk_enable_if_present <unit>
desk_enable_if_present()
{
  desk_opt "$1 is in the image and could not be enabled" \
    "systemctl cat $1 >/dev/null 2>&1 || { echo 'no $1 in this image'; exit 0; }; \
     systemctl enable $1"
}

# desk_dm [<unit>...]: graphical.target by default and a display manager that
# systemd starts at boot. The units are tried in order; with none, the package
# is trusted to have enabled its own (debian) and only the result is tested.
desk_dm()
{
  local unit
  local test_dm="test -e /etc/systemd/system/display-manager.service || \
                 test -e /etc/systemd/system/graphical.target.wants/display-manager.service"
  desk_ssh "graphical.target could not be made the default" \
           "systemctl set-default graphical.target"
  for unit in "$@"; do
    cloonix cli ${NET} ssh ${VMNAME} "${test_dm}" && break
    cloonix cli ${NET} ssh ${VMNAME} "systemctl enable --force ${unit}"
  done
  desk_ssh "no display manager is enabled (tried: ${*:-nothing, the package was to do it})" \
           "${test_dm}"
}

# desk_session <command>...: one of them must be in the image.
desk_session()
{
  desk_ssh "no desktop session in the image, none of: $*" \
    "for c in $*; do command -v \${c} && exit 0; done; exit 1"
}

# Power off, wait for the guest to be gone, then say how it went.
desk_stop()
{
  local tries
  cloonix cli ${NET} ssh ${VMNAME} "sync; sleep 1; sync"
  # Without a session bus poweroff fails on systemd 257, the fallbacks need no bus.
  cloonix cli ${NET} ssh ${VMNAME} "poweroff 2>/dev/null || \
    systemctl --force poweroff 2>/dev/null || \
    { sync; echo o > /proc/sysrq-trigger; }"
  tries=0
  while cloonix cli ${NET} dump commands 2>/dev/null | grep -q " ${VMNAME} "; do
    tries=$((tries+1))
    [ ${tries} -ge 60 ] && desk_fatal "${VMNAME} did not power off in 2 minutes"
    echo "waiting for ${VMNAME} to power off"
    sleep 2
  done
  cloonix stop ${NET}
  sleep 1
  echo "END ################################################"
  if [ -n "${DESK_WARN}" ]; then
    echo "built ${BULK}/${QCOW2}, with:${DESK_WARN}"
  else
    echo "built ${BULK}/${QCOW2}, no warning"
  fi
}
