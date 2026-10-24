#!/bin/bash
# mqtt_trixie: ../cnt_kvm/cnt_trixie.sh with mosquitto and its clients, and
# nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin mqtt_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="mosquitto,mosquitto-clients,python3"

EXTRA="netcat-openbsd psmisc socat"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/mosquitto" ]; then
  echo "ERROR: mosquitto is not in ${TRIXIE}, the mirror does not carry it"
  exit 1
fi
if [ ! -x "${TRIXIE}/usr/bin/mosquitto_sub" ]; then
  echo "ERROR: mosquitto-clients is not in ${TRIXIE}"
  exit 1
fi
demo_apt

# mosquitto keeps /var/lib/mosquitto (retained store, act 4): claimed, the demo keeps the broker root.
for i in "/etc/mosquitto" "/var/lib/mosquitto" "/var/log/mosquitto" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
fct_unshare_chroot "mkdir -p /etc/mosquitto /var/lib/mosquitto"
fct_unshare_chroot "mkdir -p /var/log/mosquitto"
fct_unshare_all "chmod 755 ${TRIXIE}/etc/mosquitto"
fct_unshare_all "chmod 1777 ${TRIXIE}/var/lib/mosquitto ${TRIXIE}/var/log/mosquitto"
# Say what is in the image.
echo "  mqtt_trixie: mosquitto and its clients in place"
demo_end
