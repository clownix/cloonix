#!/bin/bash
# kvm_autotest_tumbleweed.qcow2 from tumbleweed.qcow2 (dnf base, common part
# in kvm_autotest_common.sh); rsyslog gets /dev/log, journald forwards.
DISTRO="tumbleweed"
# No gzip: the base has busybox-gzip, which provides it and conflicts; xz
# alone after, for the same reason if a busybox-xz is there.
PKGS="python3 gcc glibc-devel shadow rsyslog dbus-1 sudo tar procps util-linux iproute2 iperf"
. "$(dirname "$0")/kvm_autotest_common.sh"

pkg_install()
{
  vm "dnf -y --nogpgcheck install ${PKGS}"
  vm "command -v xz >/dev/null || dnf -y --nogpgcheck install xz"
}

distro_setup()
{
  udev_devices
  systemd_modules
  systemd_linger
  rsyslog_syslog_file
  journald_forward
  systemd_enable rsyslog
}

autotest_build
