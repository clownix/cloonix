#!/bin/bash
# kvm_autotest_rocky.qcow2: rocky.qcow2 made ready for the autotest. Common
# part in kvm_autotest_common.sh; the base already names the mirror and has
# selinux off. Rocky's rsyslog reads the journal (imjournal).
DISTRO="rocky"
PKGS="python3 gcc glibc-devel shadow-utils rsyslog dbus sudo tar gzip xz procps-ng util-linux iproute iperf3"
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
  systemd_enable rsyslog
}

autotest_build
