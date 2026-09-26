#!/bin/bash

cnt_common_config() {
  ROOTFS="$1"
  if [ -z "${ROOTFS}" ] || [ ! -d "${ROOTFS}" ]; then
    echo "cnt_common_config: no such rootfs: ${ROOTFS}"
    return 1
  fi
  mkdir -p "${ROOTFS}/etc" "${ROOTFS}/var/log"

  SYSLOG_BIN=""
  SYSLOG_KIND=""
  for i in usr/sbin/rsyslogd sbin/rsyslogd; do
    if [ -x "${ROOTFS}/${i}" ]; then
      SYSLOG_BIN="/${i}"; SYSLOG_KIND="rsyslog"; break
    fi
  done
  if [ -z "${SYSLOG_BIN}" ]; then
    for i in usr/sbin/syslog-ng sbin/syslog-ng usr/bin/syslog-ng; do
      if [ -x "${ROOTFS}/${i}" ]; then
        SYSLOG_BIN="/${i}"; SYSLOG_KIND="syslog-ng"; break
      fi
    done
  fi

  SYSLOG_CONF=""
  if [ "${SYSLOG_KIND}" = "rsyslog" ]; then
    SYSLOG_CONF="etc/rsyslog.conf"
    mkdir -p "${ROOTFS}/var/spool/rsyslog"
    cat > "${ROOTFS}/etc/rsyslog.conf" << "EOF"
module(load="imuxsock")
$FileOwner root
$FileGroup root
$FileCreateMode 0640
$DirCreateMode 0755
$Umask 0022
$WorkDirectory /var/spool/rsyslog
*.*;auth,authpriv.none          -/var/log/syslog
EOF
  elif [ "${SYSLOG_KIND}" = "syslog-ng" ]; then
    SYSLOG_CONF="etc/syslog-ng/syslog-ng.conf"
    mkdir -p "${ROOTFS}/etc/syslog-ng" "${ROOTFS}/var/lib/syslog-ng"
    cat > "${ROOTFS}/etc/syslog-ng/syslog-ng.conf" << "EOF"
@version: 4.0

options {
  create-dirs(yes);
  dir-perm(0755);
  perm(0640);
  keep-hostname(yes);
  use-dns(no);
  chain-hostnames(no);
};

source s_local {
  unix-dgram("/dev/log");
  internal();
};

filter f_noauth { not facility(auth, authpriv); };

destination d_syslog { file("/var/log/syslog"); };

log { source(s_local); filter(f_noauth); destination(d_syslog); };
EOF
  fi

  cat > "${ROOTFS}/etc/locale.gen" << "EOF"
en_US.UTF-8 UTF-8
EOF
  cat > "${ROOTFS}/etc/locale.conf" << "EOF"
LANG=C
EOF

  # hosts.
  cat > "${ROOTFS}/etc/hosts" << "EOF"
127.0.0.1   localhost
EOF

  if [ ! -s "${ROOTFS}/etc/passwd" ]; then
    cat > "${ROOTFS}/etc/passwd" << "EOF"
root:x:0:0:root:/root:/bin/bash
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
EOF
    echo "  cnt_common: /etc/passwd was empty, a minimal one was written"
  fi
  if [ ! -s "${ROOTFS}/etc/group" ]; then
    cat > "${ROOTFS}/etc/group" << "EOF"
root:x:0:
nogroup:x:65534:
EOF
    echo "  cnt_common: /etc/group was empty, a minimal one was written"
  fi
  if ! grep -q "^root:" "${ROOTFS}/etc/passwd"; then
    echo "root:x:0:0:root:/root:/bin/bash" >> "${ROOTFS}/etc/passwd"
  fi
  if ! grep -q "^root:" "${ROOTFS}/etc/group"; then
    echo "root:x:0:" >> "${ROOTFS}/etc/group"
  fi

  # Say plainly if anything did not land, rather than leaving it to be
  # discovered by a container that behaves oddly.
  for f in ${SYSLOG_CONF} etc/locale.gen etc/locale.conf etc/hosts \
           etc/passwd etc/group; do
    if [ ! -s "${ROOTFS}/${f}" ]; then
      echo "  cnt_common: ERROR /${f} is missing or empty"
      return 1
    fi
  done

  if [ -z "${SYSLOG_BIN}" ]; then
    echo "  cnt_common: WARNING no syslog daemon in this image, so a container"
    echo "              started without an init will have no syslog at all."
    echo "              Add rsyslog to the package list of this builder, or"
    echo "              syslog-ng on a distribution that has no rsyslog."
  else
    echo "  cnt_common: ${SYSLOG_KIND} at ${SYSLOG_BIN}, config /${SYSLOG_CONF}"
  fi

  if [ -x "${ROOTFS}/usr/lib/systemd/systemd" ] || \
     [ -x "${ROOTFS}/lib/systemd/systemd" ]; then
    echo "  cnt_common: systemd present (only started with CLOONIX_WITH_SYSTEMD=1)"
  else
    echo "  cnt_common: no systemd in this image"
  fi
  echo "  cnt_common: locale, hosts and accounts in place"
  return 0
}

# The delivered form of a container image is <dir>.tar.gz in bulk: the
# server extracts it at the first use into <dir>_targz, never modified. The tree
# was built in a user namespace, so it is packed from the same kind of
# namespace, where its owners read 0..n, and erased from it afterwards.
cnt_common_pack() {
  ROOTFS="$1"
  TARGZ="${ROOTFS}.tar.gz"
  if [ -z "${ROOTFS}" ] || [ ! -d "${ROOTFS}" ]; then
    echo "cnt_common_pack: no such rootfs: ${ROOTFS}"
    return 1
  fi
  # A builder already re-executed inside its namespace is uid 0 here.
  if [ "$(id -u)" = "0" ]; then
    NS=""
  else
    NS="unshare --map-root-user --user --map-auto --"
  fi
  rm -f "${TARGZ}.tmp"
  if ! ${NS} tar --numeric-owner -C "${ROOTFS}" -czf "${TARGZ}.tmp" . ; then
    echo "cnt_common_pack: ERROR could not pack ${ROOTFS}"
    rm -f "${TARGZ}.tmp"
    return 1
  fi
  mv -f "${TARGZ}.tmp" "${TARGZ}"
  if ! ${NS} rm -rf "${ROOTFS}"; then
    chmod -R u+w "${ROOTFS}" 2>/dev/null
    rm -rf "${ROOTFS}"
  fi
  if [ -e "${ROOTFS}" ]; then
    echo "cnt_common_pack: ERROR ${ROOTFS} could not be erased after the pack"
    return 1
  fi
  echo "  cnt_common: image packed as ${TARGZ}, $(du -sh "${TARGZ}" | cut -f1)"
  return 0
}
