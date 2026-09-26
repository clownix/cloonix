#!/bin/bash
# kvm_autotest_tumbleweed.qcow2: tumbleweed.qcow2 made ready for autotest.sh.
# Common part in kvm_autotest_common.sh; the base was made with dnf and has
# it, pointed at the mirror. rsyslog gets /dev/log, so journald forwards; a
# line seen twice in /var/log/syslog means it reads the journal too.
DISTRO="tumbleweed"
# No gzip: the base has busybox-gzip, which provides it and conflicts.
PKGS="python3 gcc glibc-devel shadow rsyslog dbus-1 sudo tar procps util-linux iproute2 iperf"
. "$(dirname "$0")/kvm_autotest_common.sh"

pkg_install()
{
  vm "dnf -y --nogpgcheck install ${PKGS}"
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
