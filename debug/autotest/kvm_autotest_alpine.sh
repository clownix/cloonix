#!/bin/bash
# kvm_autotest_alpine.qcow2 from alpine.qcow2, common part in
# kvm_autotest_common.sh: openrc (no user bus, no systemd cgroups), mdev
# (device modes in local.d), busybox (GNU tools added).
DISTRO="alpine"
PKGS="bash coreutils grep sed findutils procps-ng util-linux-misc tar gzip xz \
      python3 gcc musl-dev linux-headers shadow shadow-uidmap \
      rsyslog dbus sudo iperf3"
. "$(dirname "$0")/kvm_autotest_common.sh"

pkg_install()
{
  local p
  vm "apk update"
  for p in ${PKGS}; do
    vm "apk --allow-untrusted add ${p}" || echo "WARNING: apk add ${p} failed"
  done
}

distro_setup()
{
  openrc_devices
  vm "rc-update del syslog boot 2>/dev/null; rc-service syslog stop 2>/dev/null; rc-update add rsyslog default; rc-update add dbus default"
  rsyslog_syslog_file
  vm "rc-service rsyslog restart"
}

autotest_build
