#!/bin/bash
# asterisk_trixie: ../cnt_kvm/cnt_trixie.sh with asterisk, its english prompts,
# the phones (baresip, sipp), sngrep, kamailio, nftables and openssl, and
# nothing else. Asterisk is not in trixie: asterisk_build.sh compiles it in a
# build root of the same mirror, from the asterisk tree get_asterisk.sh fills,
# and the image installs the .deb it makes like any package.
. "$(dirname "$0")/demo_common.sh"
demo_begin asterisk_trixie
AST_URL="http://127.0.0.1/asterisk"
IMAGE=${TRIXIE}
BUILD=${IMAGE}_build
for f in asterisk.tar.gz pjproject.tar.bz2; do
  if ! wget -q --spider ${AST_URL}/${f}; then
    echo "ERROR: ${AST_URL}/${f} not reachable: run get_asterisk.sh to mirror it, then check_www_links.sh to publish it"
    exit 1
  fi
done
if [ -e ${BUILD} ]; then
  fct_unshare_all "rm -rf ${BUILD}"
fi
TRIXIE=${BUILD}
LIST=$(bash "$(dirname "$0")/asterisk_build.sh" deps)
demo_strap
for f in asterisk.tar.gz pjproject.tar.bz2; do
  if ! wget -q -O ${TRIXIE}/root/${f} ${AST_URL}/${f}; then
    echo "ERROR: ${AST_URL}/${f} not fetched"
    exit 1
  fi
done
cp -f "$(dirname "$0")/asterisk_build.sh" ${TRIXIE}/root/
if ! fct_unshare_chroot "bash /root/asterisk_build.sh /root/asterisk.tar.gz /root/pjproject.tar.bz2 /root/asterisk.deb"; then
  echo "ERROR: asterisk not built in ${BUILD}"
  exit 1
fi
DEB=$(mktemp --suffix=.deb)
cp -f ${TRIXIE}/root/asterisk.deb ${DEB}
fct_unshare_all "rm -rf ${BUILD}"

TRIXIE=${IMAGE}
LIST="adduser,iputils-ping,net-tools,procps,iproute2,"
LIST+="rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="asterisk-core-sounds-en,asterisk-core-sounds-en-gsm,baresip-core,sip-tester,sngrep,kamailio,"
LIST+="nftables,netcat-openbsd,openssl,python3"
demo_strap
cp -f ${DEB} ${TRIXIE}/tmp/asterisk.deb
rm -f ${DEB}
# the .deb through apt, so its Depends come from the mirror; checked below
EXTRA="/tmp/asterisk.deb"
demo_apt
fct_unshare_chroot "rm -f /tmp/asterisk.deb"
if [ ! -x "${TRIXIE}/usr/sbin/asterisk" ]; then
  echo "ERROR: asterisk is not in ${TRIXIE}, the install of its .deb failed"
  exit 1
fi
for b in baresip sipp sngrep kamailio nft; do
  if [ ! -x ${TRIXIE}/usr/bin/${b} ] && [ ! -x ${TRIXIE}/usr/sbin/${b} ]; then
    echo "ERROR: ${b} is not in ${TRIXIE}"
    exit 1
  fi
done
# the prompts where asterisk looks for them: sounds/en is the alternative
# the postinst of asterisk-core-sounds-en makes
if ! fct_unshare_chroot "test -e /usr/share/asterisk/sounds/en/hello-world.gsm"; then
  echo "ERROR: /usr/share/asterisk/sounds/en/hello-world.gsm not in ${TRIXIE}"
  exit 1
fi
# asterisk runs as root in the container: its trees owned by root, normal modes
for i in "/etc/asterisk" "/var/lib/asterisk" "/var/spool/asterisk" \
         "/var/log/asterisk" "/var/run/asterisk" "/etc/kamailio"; do
  if [ -d "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
  fi
done
demo_end
