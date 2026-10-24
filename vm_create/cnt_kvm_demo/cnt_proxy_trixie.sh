#!/bin/bash
# proxy_trixie: ../cnt_kvm/cnt_trixie.sh with haproxy, nginx and curl, and
# nothing else.
. "$(dirname "$0")/demo_common.sh"
demo_begin proxy_trixie
# adduser is not decoration.

LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="haproxy,nginx-light,curl,python3"

EXTRA="apache2-utils netcat-openbsd socat psmisc"
demo_strap
if [ ! -x "${TRIXIE}/usr/sbin/haproxy" ]; then
  echo "ERROR: haproxy is not in ${TRIXIE}, the mirror does not carry it"
  exit 1
fi
if [ ! -x "${TRIXIE}/usr/sbin/nginx" ]; then
  echo "ERROR: nginx is not in ${TRIXIE}, so there would be nothing behind"
  exit 1
fi
demo_apt

# haproxy and nginx trees claimed; the demo tells both to stay root.
for i in "/etc/haproxy" "/var/lib/haproxy" "/etc/nginx" "/var/log/nginx" \
         "/var/www" "/var/lib/nginx" ; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
fct_unshare_chroot "mkdir -p /etc/haproxy /var/lib/haproxy /var/log/nginx"
fct_unshare_chroot "mkdir -p /var/www/html /run/haproxy"
fct_unshare_all "chmod 755 ${TRIXIE}/etc/haproxy ${TRIXIE}/etc/nginx"
fct_unshare_all "chmod 1777 ${TRIXIE}/var/log/nginx ${TRIXIE}/var/lib/haproxy"
# Say what is in the image.
echo "  proxy_trixie: haproxy and nginx in place"
demo_end
