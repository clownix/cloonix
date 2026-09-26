#!/bin/bash
# kvm_autotest_resolute.qcow2: resolute.qcow2 made ready for autotest.sh.
# Common part in kvm_autotest_common.sh; the base already names the mirror.
DISTRO="resolute"
PKGS="python3 gcc libc6-dev uidmap rsyslog dbus dbus-user-session sudo iperf3"
. "$(dirname "$0")/kvm_autotest_common.sh"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"

pkg_install()
{
  vm "${APT} update"
  vm "${APT} install --no-install-recommends ${PKGS}"
}

distro_setup()
{
  udev_devices
  systemd_modules
  systemd_linger
  adm_reads_syslog
  systemd_enable rsyslog
}

autotest_build
