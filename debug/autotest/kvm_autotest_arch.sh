#!/bin/bash
# kvm_autotest_arch.qcow2: arch.qcow2 made ready for autotest.sh. Common
# part in kvm_autotest_common.sh; the base already names the mirror. Arch
# has no rsyslog in core/extra: syslog-ng writes /var/log/syslog.
DISTRO="arch"
PKGS="python gcc shadow syslog-ng dbus sudo iperf3"
. "$(dirname "$0")/kvm_autotest_common.sh"

pkg_install()
{
  vm "pacman -Sy --noconfirm --needed ${PKGS}"
}

distro_setup()
{
  udev_devices
  systemd_modules
  systemd_linger
  syslogng_syslog_file
  systemd_enable syslog-ng@default
}

autotest_build
