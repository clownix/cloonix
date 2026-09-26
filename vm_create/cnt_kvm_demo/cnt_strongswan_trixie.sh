#!/bin/bash
# strongswan_trixie: ../cnt_kvm/cnt_trixie.sh plus strongswan, and nothing
# else.
HERE=`pwd`
DISTRO="trixie"
TRIXIE="/var/lib/cloonix/bulk/strongswan_trixie"
REPO="http://127.0.0.1/trixie"
CNTREPO="http://10.117.0.1/trixie"
MMDEBSTRAP="/usr/bin/mmdebstrap"
if [ ! -x ${MMDEBSTRAP} ]; then
  echo ${MMDEBSTRAP} not installed
  exit 1
fi
if [ -e $TRIXIE ] || [ -e $TRIXIE.tar.gz ]; then
  echo $TRIXIE or $TRIXIE.tar.gz already exists, please delete
  exit 1
fi
wget -q --spider ${REPO}
if [ $? -ne 0 ]; then
  echo ${REPO} not reachable
  exit 1
fi
fct_unshare_chroot() {
unshare --map-root-user --user --mount --wd "${TRIXIE}" \
        -- /bin/bash -c \
        "mount --rbind /proc proc ; \
         mount --rbind /sys sys ; \
         /sbin/chroot . /bin/bash -c '$1'"
}
# Same, with the whole subuid window mapped instead of root alone.
fct_unshare_all() {
unshare --map-root-user --user --map-auto -- /bin/bash -c "$1"
}
# adduser: --variant=minbase has neither adduser nor passwd, and postinsts need them.
LIST="adduser,iputils-ping,net-tools,procps,iproute2,rsyslog,strace,vim,kmod,tcpdump,iw,hostapd,wpasupplicant,"
LIST+="strongswan,strongswan-swanctl,strongswan-pki,curl,python3"

EXTRA="iperf3 netcat-openbsd psmisc conntrack"

# The local mirror's key may be unknown: sources.list entry marked trusted=yes.
unshare --map-root-user --user --map-auto --mount -- /bin/bash -c \
        "${MMDEBSTRAP} --variant=minbase \
               --include=${LIST} \
               --aptopt='Acquire::AllowInsecureRepositories \"true\"' \
               --aptopt='APT::Get::AllowUnauthenticated \"true\"' \
               ${DISTRO} ${TRIXIE} \
               \"deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main\""
if [ ! -d "${TRIXIE}" ] || [ ! -x "${TRIXIE}/usr/bin/bash" ]; then
  echo "ERROR: mmdebstrap did not produce a usable rootfs in ${TRIXIE}"
  exit 1
fi
if [ ! -x "${TRIXIE}/usr/sbin/swanctl" ]; then
  echo "ERROR: /usr/sbin/swanctl is not in ${TRIXIE}, the mirror does not"
  echo "       carry strongswan-swanctl"
  exit 1
fi
build_srcs="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${REPO} ${DISTRO} main
EOF"
fct_unshare_chroot "${build_srcs}"
# policy-rc.d exit 101: no init in this chroot, postinsts must not start daemons.
pol="cat > /usr/sbin/policy-rc.d <<EOF
#!/bin/sh
exit 101
EOF
chmod 755 /usr/sbin/policy-rc.d"
fct_unshare_chroot "${pol}"
APT_OPT="-o Acquire::AllowInsecureRepositories=true"
APT_OPT="${APT_OPT} -o APT::Get::AllowUnauthenticated=true"
# APT::Sandbox::User=root: _apt is not mapped in this namespace.
APT_OPT="${APT_OPT} -o APT::Sandbox::User=root"
# And the apt trees have to belong to root first: mmdebstrap left them owned
# by _apt, and root cannot read what an unmapped id owns.
fct_unshare_all "chown -R 0:0 ${TRIXIE}/var/lib/apt ${TRIXIE}/var/cache/apt"
fct_unshare_chroot "rm -rf /var/lib/apt/lists/partial /var/cache/apt/archives/partial"
fct_unshare_chroot "mkdir -p /var/lib/apt/lists/partial /var/cache/apt/archives/partial"
APT_LOG="${TRIXIE}/tmp/apt_extra.log"
GOT=""
MISSING=""
if fct_unshare_chroot "DEBIAN_FRONTEND=noninteractive apt-get ${APT_OPT} -y update > /tmp/apt_extra.log 2>&1"; then
  for p in ${EXTRA}; do
    if fct_unshare_chroot "DEBIAN_FRONTEND=noninteractive apt-get ${APT_OPT} -y --no-install-recommends install ${p} >> /tmp/apt_extra.log 2>&1"; then
      GOT="${GOT} ${p}"
    else
      MISSING="${MISSING} ${p}"
    fi
  done
else
  APT_BROKE=1
  echo "  WARNING apt-get update failed inside the chroot, the optional"
  echo "          packages were not attempted -- this says nothing about"
  echo "          whether the mirror has them. What apt said:"
  if [ -s "${APT_LOG}" ]; then
    sed -e "s/^/          /" "${APT_LOG}" | tail -n 12
  else
    echo "          (it said nothing at all)"
  fi
fi
fct_unshare_chroot "rm -f /usr/sbin/policy-rc.d"
fct_unshare_chroot "apt-get clean >/dev/null 2>&1"
cmd="cat > /etc/apt/sources.list <<EOF
deb [ trusted=yes allow-insecure=yes ] ${CNTREPO} ${DISTRO} main
EOF"
fct_unshare_chroot "rm -rf /var/lib/apt/lists/partial"
fct_unshare_chroot "rm -rf /var/cache/apt/archives/partial"
# The apt log was for this build, not for the guests that run the image.
fct_unshare_chroot "rm -f /tmp/apt_extra.log"
fct_unshare_chroot "${cmd}"
rm -f ${TRIXIE}/usr/bin/sh
cd ${TRIXIE}/usr/bin
ln -s /usr/bin/bash sh
cd ${HERE}
for i in "dev" "proc" "sys"; do
  chmod 755 ${TRIXIE}/${i}
done
chmod 700 ${TRIXIE}/root
# rsyslog, locale, hosts and the accounts: written into the image here, not
# by cloonixsbininit at every boot.
. "$(dirname "$0")/../cnt_kvm/cnt_common.sh"
cnt_common_config "${TRIXIE}" || exit 1
# charon runs as root in this image. It needs CAP_NET_ADMIN for XFRM and
# the demo writes into /etc/swanctl at run time, so both the config and
# the state trees are claimed for uid 0.
# Guest userns: uid 0 = the cloonix user, 1..n = subuids. A file owned outside the map is unreachable even by root (caps are gated on the owner being mapped): claim it for uid 0.
fct_unshare_chroot "mkdir -p /etc/swanctl/x509 /etc/swanctl/x509ca /etc/swanctl/private /etc/swanctl/conf.d"
for i in "/etc/swanctl" "/etc/strongswan.d" "/var/lib/strongswan" "/etc/ipsec.d" ; do
  if [ -e "${TRIXIE}${i}" ]; then
    fct_unshare_all "chown -R 0:0 ${TRIXIE}${i}"
    fct_unshare_all "chmod -R u+rwX ${TRIXIE}${i}"
  fi
done
# Say what is in the image.
echo "  strongswan_trixie: strongswan in place"
[ -n "${GOT}" ]     && echo "  strongswan_trixie: also installed -${GOT}"
if [ "${APT_BROKE}" = "1" ]; then
  echo "  strongswan_trixie: optional packages not attempted -${EXTRA}"
elif [ -n "${MISSING}" ]; then
  echo "  strongswan_trixie: not in the mirror, skipped -${MISSING}"
fi
cnt_common_pack "${TRIXIE}" || exit 1
