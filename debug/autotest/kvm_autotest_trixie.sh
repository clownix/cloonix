#!/bin/bash
# kvm_autotest_trixie.qcow2: trixie.qcow2 made ready for the autotest, the
# default machine of the autotest. Common part in kvm_autotest_common.sh.
DISTRO="trixie"
PKGS="python3 gcc libc6-dev uidmap rsyslog dbus dbus-user-session sudo iperf3 xz-utils"
. "$(dirname "$0")/kvm_autotest_common.sh"
APT="DEBIAN_FRONTEND=noninteractive DEBCONF_NONINTERACTIVE_SEEN=true \
     apt-get -o Dpkg::Options::=--force-confdef \
     -o Acquire::Check-Valid-Until=false \
     --allow-unauthenticated --assume-yes"

pkg_install()
{
  vm "cat > /etc/apt/sources.list << EOF2
deb [ trusted=yes allow-insecure=yes ] http://${MIRROR}/${DISTRO} ${DISTRO} main contrib non-free non-free-firmware
EOF2"
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
