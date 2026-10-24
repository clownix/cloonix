#!/bin/sh
EXTRACT=`pwd`
if [ $# = 1 ]; then
  IDENT=$1
else
  IDENT="nemo"
fi
ROOTFS="${EXTRACT}/rootfs"
CLOONIX_CFG="${ROOTFS}/cloonix/cloonfs/etc/cloonix.cfg"
LIST=$(cat ${CLOONIX_CFG} |grep "CLOONIX_NET:" | awk '{print $2}')
FOUND="no"
for i in ${LIST} ; do
  if [ "${IDENT}" = "${i}" ]; then
    FOUND="yes"
    break
  fi
done
web_port_of() {
  awk -v net="${IDENT}" -v key="$1" '
    ($1 == "CLOONIX_NET:") && ($2 == net) { inblock = 1; next }
    inblock && ($1 == "}") { exit }
    inblock && ($1 == key) { print $2; exit }
  ' ${CLOONIX_CFG}
}
WEB_PORT=$(web_port_of cloonix_web)
WEB_PASSWD=$(web_port_of cloonix_passwd)
if [ "${FOUND}" = "no" ]; then
  printf "\n\t ${IDENT} not found,\nlist: ${LIST}\n\n"
  exit 1
fi
BIN="${EXTRACT}/bin"
CONFIG="${EXTRACT}/config"
# setup.sh moves and patches files: it runs once per extraction. A second
# run would break what the first one made, the binaries of a running toy
# included: say what was made and leave everything as it is.
if [ -e ${CONFIG}/config.json ]; then
  DONE=$(ls ${EXTRACT}/*cmd 2>/dev/null | head -1)
  printf "\n\t./setup.sh was already run in this directory"
  if [ -n "${DONE}" ]; then
    printf ", for the net %s.\n" "$(basename ${DONE} cmd)"
    printf "\tFor another net, extract the tarball again in another directory.\n"
    sh ${DONE}
  fi
  printf "\n"
  exit 0
fi
CLOONFS="${ROOTFS}/cloonix/cloonfs"
PATCHELF="${EXTRACT}/bin/cloonix-patchelf"
HOST_BULK="/var/lib/cloonix/bulk"
USER_UID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$3}")
USER_GID=$(cat /etc/passwd |grep ^${USER} | awk -F : "{print \$4}")
PROXYSHARE_IN="/tmp/cloonix_proxymous"
PROXYSHARE_OUT="/tmp/${IDENT}_proxymous_${USER}"
PROXY="${BIN}/cloonix-proxymous-${IDENT}"
CRUN="${BIN}/cloonix-crun-${IDENT}"
mv ${CONFIG}/cloonix-init-starter-crun   ${ROOTFS}/cloonix/cloonix-init-${USER}-${IDENT}
mv ${CONFIG}/config.json.template        ${CONFIG}/config.json
mv ${BIN}/cloonix-crun                   ${BIN}/cloonix-crun-${IDENT}
mv ${BIN}/cloonix-proxymous        ${BIN}/cloonix-proxymous-${IDENT}
if [ -r ${HOST_BULK} ]; then
  sed -i s"%__HOST_BULK_PRESENT__%%g"     ${CONFIG}/config.json
else
  sed -i s"%__HOST_BULK_PRESENT__.*%%"   ${CONFIG}/config.json
fi
sed -i s"%__IDENT__%${IDENT}%g"                     ${EXTRACT}/setup.sh
sed -i s"%__IDENT__%${IDENT}%g"                     ${ROOTFS}/cloonix/cloonix-init-${USER}-${IDENT}
sed -i s"%__USER__%${USER}%g"                       ${ROOTFS}/cloonix/cloonix-init-${USER}-${IDENT}
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%g"     ${ROOTFS}/cloonix/cloonix-init-${USER}-${IDENT}
sed -i s"%__IDENT__%${IDENT}%g"                     ${CONFIG}/config.json
sed -i s"%__USER__%${USER}%g"                       ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_IN__%${PROXYSHARE_IN}%g"     ${CONFIG}/config.json
sed -i s"%__PROXYSHARE_OUT__%${PROXYSHARE_OUT}%g"   ${CONFIG}/config.json
sed -i s"%__ROOTFS__%${ROOTFS}%g"                   ${CONFIG}/config.json
sed -i s"%__USER_UID__%${USER_UID}%g"               ${CONFIG}/config.json
sed -i s"%__USER_GID__%${USER_GID}%g"               ${CONFIG}/config.json
# Toy userns: a single mapping denies setgroups (privilege-dropping daemons
# die); a second one (subuid/subgid onto 1..) needs the host's
# newuidmap/newgidmap. config/idmap says which.
subid_range() {
  # first range of $1 in $2, by name then by numeric id: "start count"
  awk -F: -v u="${USER}" -v n="$1" '$1 == u || $1 == n { print $2, $3; exit }' "$2" 2>/dev/null
}
IDMAP="single"
IDMAP_WHY=""
SUBUID=$(subid_range ${USER_UID} /etc/subuid)
SUBGID=$(subid_range ${USER_GID} /etc/subgid)
if ! command -v newuidmap >/dev/null 2>&1 || ! command -v newgidmap >/dev/null 2>&1; then
  IDMAP_WHY="newuidmap and newgidmap are not on this host (package uidmap, or shadow-utils)"
elif [ -z "${SUBUID}" ]; then
  IDMAP_WHY="/etc/subuid has no line for ${USER} (uid ${USER_UID})"
elif [ -z "${SUBGID}" ]; then
  IDMAP_WHY="/etc/subgid has no line for ${USER} (gid ${USER_GID})"
else
  SUBUID_START=$(echo ${SUBUID} | awk '{print $1}')
  SUBUID_COUNT=$(echo ${SUBUID} | awk '{print $2}')
  SUBGID_START=$(echo ${SUBGID} | awk '{print $1}')
  SUBGID_COUNT=$(echo ${SUBGID} | awk '{print $2}')
  if [ "${SUBUID_COUNT}" -lt 65536 ] || [ "${SUBGID_COUNT}" -lt 65536 ]; then
    IDMAP_WHY="the subuid or subgid range of ${USER} is shorter than 65536 (${SUBUID}, ${SUBGID})"
  else
    IDMAP="subuid"
    # the second mapping, appended to the one of the user, uids then gids
    awk -v us="${SUBUID_START}" -v uc="${SUBUID_COUNT}" \
        -v gs="${SUBGID_START}" -v gc="${SUBGID_COUNT}" '
      /"uidMappings"/ { block = "uid" }
      /"gidMappings"/ { block = "gid" }
      /"size": 1$/ && block != "" {
        print
        getline
        print "            },"
        print "            {"
        print "                \"containerID\": 1,"
        if (block == "uid") { print "                \"hostID\": " us ","; print "                \"size\": " uc }
        else                { print "                \"hostID\": " gs ","; print "                \"size\": " gc }
        print "            }"
        block = ""
        next
      }
      { print }' ${CONFIG}/config.json > ${CONFIG}/config.json.tmp
    mv -f ${CONFIG}/config.json.tmp ${CONFIG}/config.json
  fi
fi
if [ "${IDMAP}" = "subuid" ]; then
  echo "${IDMAP} ${SUBUID_START} ${SUBUID_COUNT} ${SUBGID_START} ${SUBGID_COUNT}" > ${CONFIG}/idmap
else
  echo "${IDMAP}" > ${CONFIG}/idmap
fi
for i in "cloonix-crun-${IDENT}" \
         "cloonix-proxymous-${IDENT}" \
         "cloonix-passt" ; do
  ${PATCHELF} --force-rpath --set-rpath ${BIN} ${BIN}/${i}
  ${PATCHELF} --set-interpreter ${BIN}/ld-linux-x86-64.so.2 ${BIN}/${i}
done
${PATCHELF} --add-needed ${BIN}/libzstd.so.1   ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libz.so.1      ${BIN}/cloonix-proxymous-${IDENT}
${PATCHELF} --add-needed ${BIN}/libcrypto.so.3 ${BIN}/cloonix-proxymous-${IDENT}
# Every library the three host-side binaries load is in bin: a missing one
# is a packing fault of the tarball, said here rather than at start.
for i in "cloonix-crun-${IDENT}" \
         "cloonix-proxymous-${IDENT}" \
         "cloonix-passt" ; do
  missing=$(${BIN}/ld-linux-x86-64.so.2 --list ${BIN}/${i} | grep "not found")
  if [ -n "${missing}" ]; then
    echo "ERROR: ${BIN}/${i} cannot load, this tarball lacks:"
    echo "${missing}"
    exit 1
  fi
done
if [ -e /etc/resolv.conf ]; then
  cp -f /etc/resolv.conf ${CLOONFS}/etc 
fi
pkill -f cloonix-proxymous-${IDENT} 1>/dev/null 2>&1
rm -rf ${PROXYSHARE_OUT} 1>/dev/null 2>&1
mkdir -p ${EXTRACT}/worktmp
cat > ${EXTRACT}/${IDENT}cmd << EOF
#!/bin/sh

# A host without C.UTF-8 (tumbleweed) makes bash warn here, for nothing.
export LC_ALL=C.UTF-8 2>/dev/null
IDENT="${IDENT}"
USER="${USER}"
PROXYSHARE_OUT="${PROXYSHARE_OUT}"
EXTRACT="${EXTRACT}"
WEB_PORT="${WEB_PORT}"
WEB_PASSWD="${WEB_PASSWD}"

PROXY="\${EXTRACT}/bin/cloonix-proxymous-\${IDENT}"
CRUN="\${EXTRACT}/bin/cloonix-crun-\${IDENT}"
CRUNROOT="\${EXTRACT}/worktmp"
# The demos played from the web gui run on the host, under the proxymous
# started here: they find demos/lib (demolib.py, frrlib.py) of this toy
# through this, not the /usr/share/cloonix/demos of a bundle on the host.
export CLOONIX_DEMO_DIR="\${EXTRACT}/rootfs/cloonix/cloonfs/share/cloonix/demos"
# No cgroup of its own: the toy stays in the cgroup it is started from, beside
# its proxymous, the same on every host. A kernel (RHEL 10, 6.12) starves for
# tens of seconds what runs outside a busy cgroup, proxymous first, and the
# net then destroys itself. The price: systemd as pid 1 of a container of the
# toy has no cgroup to write in, the starter says so and runs without it.
# CLOONIX_TOY_CGROUPM=systemd asks the host's systemd user manager for one.
CGROUPM_ON="--cgroup-manager=\${CLOONIX_TOY_CGROUPM:-disabled}"
CGROUPM="--cgroup-manager=disabled"

# The demos and everything else are played from the web gui.
web_gui() {
  host_ip=\$(ip -4 route get 1.1.1.1 2>/dev/null | \\
            sed -n 's/.* src \\([0-9.]*\\).*/\\1/p')
  printf "\\n\\tTHE WEB GUI of \${IDENT}, the demos are in its Demo button:\\n"
  printf "https://127.0.0.1:\${WEB_PORT}  or  https://\${host_ip:-<address of this machine>}:\${WEB_PORT}"
  printf "   user: \${IDENT}   password: \${WEB_PASSWD}\\n\\n"
}

stop_toy() {
  \${CRUN} \${CGROUPM} --root=\${CRUNROOT} kill \${IDENT} 9 2>/dev/null
  sleep 1
  \${CRUN} \${CGROUPM} --root=\${CRUNROOT} delete \${IDENT} 2>/dev/null
  # The container init survives when crun lost track of the container; pid 1
  # of a pid namespace drops SIGTERM.
  left=\$(ps -axo pid,args | grep "cloonix-init-\${USER}-\${IDENT}" | \\
         grep -v grep | awk '{print \$1}')
  if [ ! -z "\${left}" ]; then
    echo "the container init is still there, killing it: \${left}"
    kill -9 \${left} 2>/dev/null
    sleep 1
  fi
  # The state directory too ("directory already exists" on the next start).
  # Files written by an inner uid belong to a subuid: rm from the same map.
  if [ "\$(head -c 6 \${EXTRACT}/config/idmap 2>/dev/null)" = "subuid" ]; then
    unshare --map-root-user --map-auto rm -rf \${CRUNROOT}/\${IDENT} 2>/dev/null || \\
      rm -rf \${CRUNROOT}/\${IDENT}
  else
    rm -rf \${CRUNROOT}/\${IDENT}
  fi
  rm -rf \${PROXYSHARE_OUT}
  left=\$(ps -axo pid,args | grep -E "cloonix-init-\${USER}-\${IDENT}|\${PROXY}" | \\
         grep -v grep)
  if [ -z "\${left}" ]; then
    echo "\${IDENT} stopped, nothing of it is left"
  else
    echo "WARNING these are still running:"
    echo "\${left}"
  fi
}

usage_cmd() {
  printf "\\n\\tCHOICE:\\n"
  printf "./${IDENT}cmd start\\n"
  printf "./${IDENT}cmd stop\\n"
  printf "./${IDENT}cmd shell\\n"
  web_gui
  exit 
}

if [ \${#} -ne 1 ]; then
  usage_cmd
fi
cmd=\$1

case \$cmd in

  start)
    # A bundle of the same net already running shares its doors port, work
    # directory and /tmp with this toy, and the server inside would never be
    # ready: look first. /tmp/cloonix_proxymous_<net> or a cloonix-main-server
    # carrying the net is the bundle's; the toy's files are under \${EXTRACT}.
    bundle_share="/tmp/cloonix_proxymous_\${IDENT}"
    bundle_proc=\$(ps -axo args | grep "cloonix-main-server" | \\
                  grep -w "\${IDENT}" | grep -v grep)
    if [ -d "\${bundle_share}" ] || [ ! -z "\${bundle_proc}" ]; then
      echo
      echo "ERROR: a cloonix net called \${IDENT} is ALREADY RUNNING on this"
      echo "       machine, from the system installation rather than from"
      echo "       this toy directory."
      echo
      if [ -d "\${bundle_share}" ]; then
        echo "       its share directory: \${bundle_share}"
      fi
      if [ ! -z "\${bundle_proc}" ]; then
        echo "       its server: \${bundle_proc}"
      fi
      echo
      echo "       The two cannot both have the net \${IDENT}: they share its"
      echo "       doors port and its work directory, and the second one to"
      echo "       start comes up half way and then waits for ever."
      echo
      echo "       Either stop that one:"
      echo "cloonix stop \${IDENT}"
      echo "       or give this toy a different name:"
      echo "./setup.sh <another name from cloonix.cfg>"
      echo
      exit 1
    fi
    exists=\$(ps -axo pid,args | grep  \${PROXY} | grep -v grep)
    if [ ! -z "\$exists" ]; then
      echo Error, process already exists:
      echo "\$exists"
      exit 1
    fi
    # What a cloonix stop leaves, or a crash: cleared, not refused.
    if [ -e \${EXTRACT}/worktmp/\${IDENT} ] || \\
       [ ! -z "\$(ps -axo args | grep cloonix-init-\${USER}-\${IDENT} | grep -v grep)" ]; then
      echo "leftovers of a previous run, clearing them"
      stop_toy
    fi
    # The certificate is config/, made when the tarball was built, yours to
    # replace (a new one per start broke the browser trust). A config/ without
    # the pair is filled by proxymous at its first start.
    \${PROXY} \${PROXYSHARE_OUT} \${IDENT}
    echo "crun create with \${CGROUPM_ON}"
    if ! \${CRUN} \${CGROUPM_ON} \\
             --root=\${EXTRACT}/worktmp \\
             --debug \\
             --log=\${EXTRACT}/worktmp/crun.log \\
             create -f \${EXTRACT}/config/config.json \${IDENT}; then
      echo "Fail: crun create, see \${EXTRACT}/worktmp/crun.log"
      pkill -f cloonix-proxymous-\${IDENT}
      exit 1
    fi
    if ! \${CRUN} \${CGROUPM} \\
             --root=\${EXTRACT}/worktmp \\
             --debug \\
             --log=\${EXTRACT}/worktmp/crun.log \\
             start \${IDENT}; then
      echo "Fail: crun start, see \${EXTRACT}/worktmp/crun.log"
      pkill -f cloonix-proxymous-\${IDENT}
      exit 1
    fi
    count_loop=0
    while [ ! -e "\${PROXYSHARE_OUT}/proxymous_start_status" ]; do
      sleep 1
      count_loop=\$((count_loop+1))
      if [ \$count_loop -gt 0 ]; then
        echo Waiting for \${PROXY} launching, count \$count_loop.
      fi
      if [ \$count_loop -gt 3 ]; then
        echo Fail launching:
        echo \${PROXY} \${PROXYSHARE_OUT} \${IDENT}
        exit 1
      fi
    done
    echo \${PROXY} launched and OK
    while [ -z "\$(grep server_is_ready \${PROXYSHARE_OUT}/proxymous_start_status)" ]; do
      count_loop=\$((count_loop+1))
      if [ \$count_loop -gt 10 ]; then
        echo Fail waiting for server look at:
        echo \${EXTRACT}/worktmp/crun.log
        echo \${EXTRACT}/rootfs/root/cloonix-init-starter-crun.log
        echo \${EXTRACT}/rootfs/cloonix/cloonfs/var/log/syslog
        exit 1
      fi
      cat \${PROXYSHARE_OUT}/proxymous_start_status 
      echo \$count_loop
      sleep 3
    done
    echo server started and ready
    web_gui
  ;;

  stop)
    stop_toy
  ;;

  shell)
    \${CRUN} \${CGROUPM} --root=\${CRUNROOT} exec -ti \${IDENT} /bin/bash 
  ;;

  *)
    usage_cmd
  ;;
esac
EOF
chown -R ${USER}:${USER} ${EXTRACT}
chmod -R u-s ${EXTRACT}
chmod -R g-s ${EXTRACT}
chmod -R o-s ${EXTRACT}
if [ -d ${CLOONFS}/share/cloonix ]; then
  chmod 755  ${CLOONFS}/share/cloonix
  chmod 1777 ${CLOONFS}/share/cloonix/demos ${CLOONFS}/share/cloonix/pcap
  for i in ${CLOONFS}/share/cloonix/demos/*; do
    [ -e "${i}" ] || continue
    if [ -d "${i}" ]; then
      chmod -R 755 "${i}"
    else
      chmod 644 "${i}"
    fi
  done
fi
# A specialised toy has no kvm and says nothing about it.
if [ -e ${CLOONFS}/bin/cloonix-qemu-system ]; then
  if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then 
    printf "\n\t/dev/kvm usable as ${USER}"
    if [ -r /dev/vhost-net ] && [ -w /dev/vhost-net ]; then 
      printf "\n\t/dev/vhost-net usable as ${USER}"
      if [ -r /dev/net/tun ] && [ -w /dev/net/tun ]; then 
        printf "\n\t/dev/net/tun usable as ${USER}"
        printf "\n\t${USER} should be able to use kvm (qemu)"
        if [ -r "/var/lib/cloonix/bulk/trixie.qcow2" ]; then
          printf "\n\ttrixie.qcow2 exists in bulk, kvm demos should work"
        else
          printf "\n\ttrixie.qcow does not exist in bulk, kvm demos will not work"
        fi
      else
        printf "\n\t/dev/net/tun not usable, to use qemu try:"
        printf "\n\tchmod 666 /dev/net/tun"
      fi
    else
      printf "\n\t/dev/vhost-net not usable, to use qemu try:"
      printf "\n\tusermod -a -G kvm ${USER}"
    fi
  else
    printf "\n\t/dev/kvm not usable, to use qemu try:"
    printf "\n\tusermod -a -G kvm ${USER}"
  fi
fi
if [ "${IDMAP}" = "subuid" ]; then
  printf "\n\tuser namespace: ${USER} onto 0 and subuid ${SUBUID_START}+${SUBUID_COUNT} onto 1..,"
  printf "\n\t                written by newuidmap/newgidmap of the host: every daemon runs"
  printf "\n\t                to erase this directory later: unshare --map-root-user --map-auto rm -rf ${EXTRACT}"
else
  printf "\n\tuser namespace: ${USER} onto 0 only, because ${IDMAP_WHY}"
  printf "\n\t                setgroups is denied inside: named, dnsmasq, postfix, mosquitto,"
  printf "\n\t                nginx, haproxy, asterisk cannot drop their privileges and die at start;"
  printf "\n\t                the demos address/dns, address/dhcp, services/* will not run here"
  printf "\n\t                (sudo usermod --add-subuids 100000-165535 --add-subgids 100000-165535 ${USER},"
  printf "\n\t                 then ./setup.sh ${IDENT} again in a fresh extraction)"
fi
if lsmod | grep -wq mac80211_hwsim; then
  printf "\n\tmac80211_hwsim loaded wifi emulation should work"
else
  printf "\n\tmac80211_hwsim not loaded wifi emulation will not work"
fi
printf "\n\n"
chmod +x ${EXTRACT}/${IDENT}cmd
sh ${EXTRACT}/${IDENT}cmd
