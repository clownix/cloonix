# kvm_autotest_common.sh - sourced by the kvm_autotest_<distro>.sh builders
# and by autotest.sh. Functions only, no side effect.
#
# A builder sets DISTRO and PKGS, defines pkg_install and distro_setup, and
# calls autotest_build: <distro>.qcow2 is copied to kvm_autotest_<distro>.qcow2,
# booted on nemo with a nat to the mirror 10.117.0.1, written into, and
# powered off. What every autotest machine gets, whatever the distro:
#   PKGS                  TOOLS below, a libc to compile with, rsyslog, dbus
#   user                  created when the base has none
#   sudoers.d/user        passwordless sudo: fail_spider.sh, the tap check
#   subuid, subgid        user:100000:65536
#   /etc/hosts            the hostname, so sudo says nothing
#   /var/lib/cloonix/bulk mode 777
#   no firewall           FIREWALL_OFF below
# then distro_setup, built from the named mechanisms below: which devices
# tool sets kvm/vhost-net/tun to 0666, how vhost_net is loaded, how user@
# and its bus exist with no login, how /var/log/syslog gets written (rsyslog, or syslog-ng where there
# is no rsyslog).
NET="${NET:-nemo}"
BULK="/var/lib/cloonix/bulk"
VMNAME="pod"
MIRROR="10.117.0.1"
# What the suites and the runner call on the machine; the builder refuses
# a machine missing one, autotest.sh refuses to push to one.
TOOLS="bash tar gzip python3 gcc sudo newuidmap unshare setsid nohup pgrep pkill timeout ip truncate"
# The tap flow of testbigflows.py is measured only with this one.
TOOLS_OPT="iperf3"

# Run as root in a machine, by the builder and again by autotest.sh for a
# machine built before: every firewall service found is stopped, disabled
# and masked, its rules flushed, its name printed.
FIREWALL_OFF='
for s in firewalld ufw nftables iptables ip6tables; do
  if command -v systemctl >/dev/null 2>&1; then
    systemctl is-enabled -q $s 2>/dev/null || systemctl is-active -q $s 2>/dev/null || continue
    systemctl disable --now $s >/dev/null 2>&1
    systemctl mask $s >/dev/null 2>&1
    echo "firewall: $s found, stopped, disabled, masked"
  elif command -v rc-update >/dev/null 2>&1; then
    rc-update show | grep -qw $s || continue
    rc-service $s stop >/dev/null 2>&1
    rc-update del $s >/dev/null 2>&1
    echo "firewall: $s found, stopped, removed from the runlevels"
  fi
done
if command -v ufw >/dev/null 2>&1; then ufw disable >/dev/null 2>&1; fi
if command -v nft >/dev/null 2>&1; then nft flush ruleset 2>/dev/null; fi
for t in iptables ip6tables; do
  command -v $t >/dev/null 2>&1 || continue
  $t -P INPUT ACCEPT 2>/dev/null; $t -P FORWARD ACCEPT 2>/dev/null; $t -F 2>/dev/null
done
true'

vm() { cloonix cli ${NET} ssh ${VMNAME} "$@"; }

# Stop the net and WAIT for it to be gone, then start it and wait until it
# answers: the lock outlives the process by a second or two.
net_restart()
{
  local tries=0
  set +e
  if cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; then
    cloonix stop ${NET}
  fi
  while cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server; do
    tries=$((tries+1))
    [ ${tries} -ge 30 ] && { echo "FATAL: ${NET} will not stop"; exit 1; }
    sleep 2
  done
  tries=0
  while true; do
    cloonix start ${NET} 2>&1 | grep -v "^$"
    sleep 2
    cloonix cli ${NET} dump pid 2>/dev/null | grep -q cloonix_server && break
    tries=$((tries+1))
    [ ${tries} -ge 15 ] && { echo "FATAL: ${NET} will not start"; exit 1; }
    sleep 2
  done
}

# ---- mechanisms a distro_setup picks from ---------------------------------

# udev rules: the nested kvm devices for everybody.
udev_devices()
{
  vm "cat > /etc/udev/rules.d/80-autotest.rules << EOF2
KERNEL==\"kvm\", MODE=\"0666\"
KERNEL==\"vhost-net\", MODE=\"0666\"
KERNEL==\"tun\", MODE=\"0666\"
EOF2"
}

# openrc has mdev, not udev: the modes are set by a local.d script at boot.
openrc_devices()
{
  vm "cat > /etc/local.d/autotest.start << EOF2
#!/bin/sh
modprobe vhost_net 2>/dev/null
chmod 666 /dev/kvm /dev/vhost-net /dev/net/tun 2>/dev/null
exit 0
EOF2
chmod 755 /etc/local.d/autotest.start; rc-update add local default"
}

systemd_modules() { vm "echo vhost_net > /etc/modules-load.d/vhost_net.conf"; }

# user@<uid> and its bus with no login: crun's cgroup manager for the toy.
systemd_linger()
{
  vm "loginctl enable-linger user || \
      { mkdir -p /var/lib/systemd/linger; touch /var/lib/systemd/linger/user; }"
}

# A unit that will not start says why before the build fails.
systemd_enable()
{
  vm "systemctl enable --now $1 || { systemctl status $1 --no-pager -l; \
      journalctl -u $1 -n 20 --no-pager; exit 1; }"
}

# A line sent through syslog(3) must land in /var/log/syslog and be
# readable by user, who tails it, or the suites cannot be read on this
# machine.
syslog_check()
{
  vm "python3 -c 'import syslog; syslog.syslog(\"autotest-probe\")'; sleep 2; \
      grep -q autotest-probe /var/log/syslog || \
      { echo 'ERROR: nothing reaches /var/log/syslog'; exit 1; }; \
      su user -c 'grep -q autotest-probe /var/log/syslog' || \
      { ls -l /var/log/syslog; echo 'ERROR: user cannot read /var/log/syslog'; exit 1; }"
}

# Debian's rsyslog already writes /var/log/syslog, root:adm 0640.
adm_reads_syslog() { vm "usermod -aG adm user"; }

# Elsewhere rsyslog writes /var/log/messages: one more file, readable. The
# unit's umask (0066 on RHEL) would mask the mode of the rule.
rsyslog_syslog_file()
{
  vm "mkdir -p /etc/systemd/system/rsyslog.service.d; \
      printf '[Service]\nUMask=0022\n' > /etc/systemd/system/rsyslog.service.d/autotest.conf; \
      systemctl daemon-reload 2>/dev/null; true"
  vm "mkdir -p /etc/rsyslog.d; \
      echo '*.* action(type=\"omfile\" file=\"/var/log/syslog\" fileCreateMode=\"0644\")' \
        > /etc/rsyslog.d/50-autotest.conf; \
      grep -q 'rsyslog.d' /etc/rsyslog.conf || \
        echo 'include(file=\"/etc/rsyslog.d/*.conf\" mode=\"optional\")' >> /etc/rsyslog.conf"
}

# syslog-ng, where the distro has no rsyslog: its system() source reads the
# journal, so nothing to forward; the config is checked before the machine
# is kept.
syslogng_syslog_file()
{
  vm "src=\$(sed -n 's/^source[[:space:]]*\([A-Za-z0-9_]*\).*/\1/p' /etc/syslog-ng/syslog-ng.conf | head -1); \
      [ -n \"\$src\" ] || { echo 'ERROR: no source in syslog-ng.conf'; exit 1; }; \
      printf 'destination d_autotest { file(\"/var/log/syslog\" perm(0644)); };\nlog { source(%s); destination(d_autotest); };\n' \"\$src\" \
        >> /etc/syslog-ng/syslog-ng.conf; \
      syslog-ng -s"
}

# rsyslog reading /dev/log through imuxsock gets nothing from journald
# unless journald forwards.
journald_forward()
{
  vm "mkdir -p /etc/systemd/journald.conf.d; \
      printf '[Journal]\nForwardToSyslog=yes\n' > /etc/systemd/journald.conf.d/autotest.conf; \
      systemctl restart systemd-journald"
}

# ---- the build ------------------------------------------------------------

boot_with_nat()
{
  local tries=0
  cp -vf ${BULK}/${SRC} ${BULK}/${QCOW2}
  sync
  cloonix cli ${NET} add kvm ${VMNAME} ram=2000 cpu=2 eth=1 ${QCOW2} --persistent
  while ! vm "echo" >/dev/null 2>&1; do
    tries=$((tries+1))
    [ ${tries} -ge 100 ] && { echo "FATAL: ${VMNAME} never answered ssh"; exit 1; }
    echo "${VMNAME} not ready, waiting 3 sec"
    sleep 3
  done
  cloonix cli ${NET} add nat nat
  cloonix cli ${NET} plug nat lan_nat
  cloonix cli ${NET} plug ${VMNAME} eth0 lan_nat
  # Whichever dhcp client the base has, each bringing the link up itself;
  # a base whose service already did it answers the ping below.
  vm "dhclient -4 eth0 2>/dev/null || dhcpcd -4 eth0 2>/dev/null || \
      udhcpc -i eth0 -n -q 2>/dev/null; true"
  tries=0
  while ! vm "ping -c1 -W2 ${MIRROR} >/dev/null 2>&1"; do
    tries=$((tries+1))
    [ ${tries} -ge 20 ] && { echo "FATAL: mirror ${MIRROR} not reachable"; exit 1; }
    echo "network not ready, waiting 3 sec"
    sleep 3
  done
}

# The tools by name, and gcc with a libc to link: a missing one fails the
# build, its name printed.
machine_check()
{
  vm "rc=0; for t in ${TOOLS}; do command -v \$t >/dev/null 2>&1 || { echo MISSING \$t; rc=1; }; done; \
      for t in ${TOOLS_OPT}; do command -v \$t >/dev/null 2>&1 || echo \"WARNING: no \$t, optional\"; done; \
      echo 'int main(void){return 0;}' > /tmp/cc.c; \
      gcc -o /tmp/cc /tmp/cc.c || { echo 'MISSING a libc to compile with'; rc=1; }; \
      rm -f /tmp/cc /tmp/cc.c; exit \$rc"
}

common_setup()
{
  machine_check
  vm "id user >/dev/null 2>&1 || useradd -m -s /bin/bash user"
  vm "mkdir -p /etc/sudoers.d; \
      echo 'user ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/user; chmod 440 /etc/sudoers.d/user; \
      grep -qE '^[@#]includedir /etc/sudoers.d' /etc/sudoers || echo '@includedir /etc/sudoers.d' >> /etc/sudoers"
  vm "grep -q '^user:' /etc/subuid || echo 'user:100000:65536' >> /etc/subuid; \
      grep -q '^user:' /etc/subgid || echo 'user:100000:65536' >> /etc/subgid"
  vm "h=\$(uname -n); grep -q \"\$h\" /etc/hosts || echo \"127.0.1.1 \$h\" >> /etc/hosts"
  vm "mkdir -p ${BULK}; chmod 755 /var/lib/cloonix; chmod 777 ${BULK}"
  vm "${FIREWALL_OFF}"
}

# Clean stop, verified by the vm leaving the topology: over the agent there
# is no session bus, poweroff on systemd 257 fails, the fallbacks do not
# need the bus.
poweroff_and_kill()
{
  local tries=0
  set +e
  trap - ERR
  vm "rm -f /etc/resolv.conf; sync; sleep 1; sync"
  vm "poweroff 2>/dev/null || systemctl --force poweroff 2>/dev/null || \
      { sync; echo o > /proc/sysrq-trigger; }"
  while cloonix cli ${NET} dump commands 2>/dev/null | grep -q " ${VMNAME} "; do
    tries=$((tries+1))
    [ ${tries} -ge 30 ] && { echo "FATAL: ${VMNAME} did not power off, ${QCOW2} is not usable"; exit 1; }
    echo "waiting for ${VMNAME} to power off"
    sleep 2
  done
  sleep 2
  cloonix stop ${NET}
  sleep 1
  echo "${BULK}/${QCOW2} done"
}

autotest_build()
{
  SRC="${DISTRO}.qcow2"
  QCOW2="kvm_autotest_${DISTRO}.qcow2"
  [ -e ${BULK}/${SRC} ] || \
    { echo "ERROR: ${BULK}/${SRC} not found: vm_create/cnt_kvm/doitall.sh --kvm ${DISTRO}"; exit 1; }
  net_restart
  boot_with_nat
  # A machine left half written would pass for a finished one.
  trap 'echo "FATAL: ${QCOW2} not made, erased"; cloonix stop ${NET}; rm -f ${BULK}/${QCOW2}' ERR
  set -E
  set -e
  pkg_install
  common_setup
  distro_setup
  syslog_check
  poweroff_and_kill
}
