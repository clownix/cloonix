#!/bin/bash
HERE=`pwd`
set -e
CLOONIX_CFG="/usr/libexec/cloonix/cloonfs/etc/cloonix.cfg"
if [ ! -e ${CLOONIX_CFG} ]; then
  echo NOT FOUND:
  echo ${CLOONIX_CFG}
  exit 1
fi
NAME="cloonix-toy"
EXTRACT="/tmp/${NAME}"
PATCHELF="/usr/libexec/cloonix/cloonfs/bin/cloonix-patchelf"
CRUN="/usr/libexec/cloonix/cloonfs/bin/cloonix-crun"
PROXY="/usr/libexec/cloonix/cloonfs/bin/cloonix-proxymous"
PASST="/usr/libexec/cloonix/cloonfs/bin/cloonix-passt"
LD="/usr/libexec/cloonix/cloonfs/lib64/ld-linux-x86-64.so.2"
COMMON_LIBS="/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu"
INIT_CRUN="${HERE}/tools_crun/cloonix-init-starter-crun"
TEMPLATE="${HERE}/tools_crun/config.json.template"
SETUP="${HERE}/tools_crun/setup.sh"
CLOONIX="${EXTRACT}/rootfs/cloonix"
CONFIG="${EXTRACT}/config"
BIN="${EXTRACT}/bin"
for i in ${CRUN} ${LD} ${PROXY} ${PASST} ${PATCHELF} \
         ${TEMPLATE} ${INIT_CRUN} ${SETUP} ; do
  if [ ! -e ${i} ]; then 
    echo ${i} missing 
    exit 1
  fi
done
# Libraries of the four host-side binaries, by the cloonfs loader, plus what
# setup.sh adds to proxymous and patchelf; passt: the nat runs host-side.
LISTSO="${COMMON_LIBS}/libzstd.so.1 ${COMMON_LIBS}/libz.so.1 \
        ${COMMON_LIBS}/libcrypto.so.3 ${COMMON_LIBS}/libm.so.6"
for b in ${CRUN} ${PROXY} ${PASST} ${PATCHELF}; do
  OUTSIDE=$(${LD} --list ${b} | awk '$2 == "=>" && $3 !~ /^\/usr\/libexec\/cloonix\/cloonfs\// {print $1, $3}')
  if [ -n "${OUTSIDE}" ]; then
    echo "${b} loads from the host: ${OUTSIDE}"
    exit 1
  fi
  LISTSO="${LISTSO} $(${LD} --list ${b} | awk '$2 == "=>" {print $3}')"
done
LISTSO=$(echo ${LISTSO} | tr ' ' '\n' | sort -u)
for i in ${LISTSO}; do
  if [ ! -e "${i}" ]; then 
    echo ${i} missing 
    exit 1
  fi
done
if [ -e ${EXTRACT} ]; then 
  echo ${EXTRACT} already exists, removing it 
  rm -rf ${EXTRACT}
fi
cd ${HERE}
mkdir -p ${CLOONIX}
for i in "cloonfs" "insider_agents" "startup_bin"; do
  unshare -rm cp -rf /usr/libexec/cloonix/${i} ${CLOONIX}
done
mkdir -p ${CLOONIX}/cloonfs/share/cloonix/demos
mkdir -p ${CLOONIX}/cloonfs/share/cloonix/pcap
chmod 755  ${CLOONIX}/cloonfs/share/cloonix
chmod 1777 ${CLOONIX}/cloonfs/share/cloonix/demos \
           ${CLOONIX}/cloonfs/share/cloonix/pcap
for i in ${CLOONIX}/cloonfs/share/cloonix/demos/*; do
  [ -e "${i}" ] || continue
  if [ -d "${i}" ]; then
    chmod -R 755 "${i}"
  else
    chmod 644 "${i}"
  fi
done
# The demos of toy_drop are left out: their files, packages and voice lines,
# their clips; a package left without demo goes.
DROP=$(grep -v "^#" ${HERE}/toy_drop | awk '{print $1}' | tr '\n' ' ')
cd ${CLOONIX}/cloonfs/share/cloonix/demos
for f in ${DROP}; do
  rm -f "${f}" "${CLOONIX}/cloonfs/bin/jscloonix/audio/${f%.py}.ogg"
done
awk -v drop="${DROP}" '
  BEGIN { n = split(drop, d, " "); for (i = 1; i <= n; i++) gone[d[i]] = 1 }
  /^#/ { print; next }
  /^$/ { next }
  /^[0-9]+[ \t]/ { pending = $0; next }
  ($1 in gone) { next }
  { if (pending != "") { print ""; print pending; pending = "" } print }' \
  packages > packages.tmp
mv -f packages.tmp packages
awk -v drop="${DROP}" '
  BEGIN { n = split(drop, d, " "); for (i = 1; i <= n; i++) gone[d[i]] = 1 }
  !($1 in gone)' voice > voice.tmp
mv -f voice.tmp voice
chmod 644 packages voice
cd ${HERE}
NB_DEMO=$(find ${CLOONIX}/cloonfs/share/cloonix/demos -maxdepth 3 -type f \
               -not -path "*/demos/lib/*" \
               \( -name "*.py" -o -name "*.sh" \) 2>/dev/null | wc -l)
if [ "${NB_DEMO}" = "0" ]; then
  echo "WARNING: no demo was found in the toy"
else
  echo "  ${NB_DEMO} demos in share/cloonix/demos, seen as /usr/share/cloonix/demos"
fi
mkdir -p ${BIN}
cp -f ${CLOONIX}/cloonfs/bin/cloonix ${BIN}/cloonix
cp -f ${CLOONIX}/cloonfs/bin/cloonix_*.py ${BIN}
chmod 755 ${BIN}/cloonix ${BIN}/cloonix_*.py
# One configuration: the client's cloonix_config.py is the server's cloonix.cfg.
ln -sf ../rootfs/cloonix/cloonfs/etc/cloonix.cfg ${BIN}/cloonix_config.py
cp -f ${CLOONIX}/cloonfs/bin/make_cloonix_cert ${BIN}/make_cloonix_cert
chmod 755 ${BIN}/make_cloonix_cert
mkdir -p ${CONFIG}
if [ -x ${BIN}/make_cloonix_cert ]; then
  ${BIN}/make_cloonix_cert ${CONFIG} cloonix-toy
  if [ ! -s ${CONFIG}/cloonix_cert.pem ]; then
    echo "WARNING no certificate made, this toy will serve http only"
  fi
else
  echo "WARNING make_cloonix_cert is missing, this toy will serve http only"
fi
cat > ${CLOONIX}/cloonfs/bin/cloonix << "EOF"
#!/bin/sh
# cloonix, container edition: ssh and start, without python.
CLOONFS="/cloonix/cloonfs"
CXP="${CLOONFS}/bin/cloonix-cxp-cli"
CFG="${CLOONFS}/etc/cloonix.cfg"
AWK="${CLOONFS}/bin/awk"

# PROXYSHARE_IN of glob_common.h. It is written out here because sh cannot
# read a C header; if it ever moves, it moves in both places.
PROXYSHARE="/tmp/cloonix_proxymous"

# One field out of the block of one net in the config.
net_field()
{
  ${AWK} -v n="$1" -v k="$2" '
    $1=="CLOONIX_NET:" { inside = ($2==n) }
    inside && $1==k    { print $2; exit }' ${CFG}
}

verb=$1
[ $# -ge 1 ] && shift
case "${verb}" in
  ssh)
    if [ $# -lt 2 ]; then
      echo "usage: cloonix cli <net> ssh <vm> [command]" >&2
      exit 1
    fi
    net=$1; vm=$2; shift 2
    pass=$(net_field ${net} cloonix_passwd)
    if [ -z "${pass}" ]; then
      echo "no net called '${net}' in ${CFG}" >&2
      exit 1
    fi
    # The proxy socket when there is one, the tcp address otherwise. Same
    # test as web_pty.c: a net running in this container is reached through
    # its socket, another one only by its address.
    sock="${PROXYSHARE}_${net}/proxy_pmain.sock"
    if [ -S "${sock}" ]; then
      target=${sock}
    else
      target="$(net_field ${net} cloonix_ip):$(net_field ${net} cloonix_port)"
    fi
    if [ $# -eq 0 ]; then
      CLOONIX_PASSWD=${pass} exec ${CXP} ${target} - ${vm}
    else
      CLOONIX_PASSWD=${pass} exec ${CXP} ${target} - ${vm} "$*"
    fi
    ;;
  start)
    if [ $# -lt 1 ]; then
      echo start wants a net name:
      ${CLOONFS}/bin/grep CLOONIX_NET: ${CFG} | ${AWK} '{print $2}'
      exit 1
    fi
    exec ${CLOONFS}/bin/cloonix-main-server ${CFG} $1
    ;;
  list|stop|cli|add|del|plug|unplug|cnf|dump|sniff|save|rpc|demo|scp|files|trace)
    echo "cloonix ${verb} is not run from inside: the toy is driven from" >&2
    echo "its web gui, given by ./<net>cmd on the host." >&2
    exit 1
    ;;
  *)
    echo "usage: cloonix start <net> | cloonix cli <net> ..." >&2
    exit 1
    ;;
esac
EOF
chmod 755 ${CLOONIX}/cloonfs/bin/cloonix
rm -f ${CLOONIX}/cloonfs/bin/cloonix_*.py
mkdir -p ${CONFIG}
mkdir -p ${BIN}
for i in ${LISTSO}; do
  cp -f ${i} ${BIN} 
done
for i in ${LD} ${CRUN} ${PROXY} ${PASST} ${PATCHELF} ; do
  cp -f ${i} ${BIN}
done
cp -f  ${SETUP}     ${EXTRACT}
cp -f  ${TEMPLATE}  ${CONFIG}
cp -f  ${INIT_CRUN} ${CONFIG}
${PATCHELF} --force-rpath --set-rpath ./bin                      ${BIN}/cloonix-patchelf
${PATCHELF} --set-interpreter         ./bin/ld-linux-x86-64.so.2 ${BIN}/cloonix-patchelf
${PATCHELF} --add-needed              ./bin/libm.so.6            ${BIN}/cloonix-patchelf
LDLINUX="/cloonix/cloonfs/lib64/ld-linux-x86-64.so.2"
RPATH_LIB="/cloonix/cloonfs/lib:/cloonix/cloonfs/lib/x86_64-linux-gnu"
for i in $(ls ${CLOONIX}/startup_bin) ; do
  ${PATCHELF} --force-rpath --set-rpath ${RPATH_LIB} ${CLOONIX}/startup_bin/${i} 2>/dev/null
  ${PATCHELF} --set-interpreter ${LDLINUX} ${CLOONIX}/startup_bin/${i} 2>/dev/null
done
set +e
LDLINUX="/lib64/ld-linux-x86-64.so.2"
RPATH_LIB="/lib:/lib/x86_64-linux-gnu"
for j in "bin" \
         "bin/jscloonix" \
         "sbin" \
         "lib" \
         "lib/systemd" \
         "lib/x86_64-linux-gnu" \
         "lib/x86_64-linux-gnu/rsyslog" \
         "lib/x86_64-linux-gnu/systemd" ; do
  if [ -e ${CLOONIX}/cloonfs/${j} ]; then
    for i in `find ${CLOONIX}/cloonfs/${j} -maxdepth 1 -type f` ; do
      ${PATCHELF} --force-rpath --set-rpath ${RPATH_LIB} ${i} 2>/dev/null
      ${PATCHELF} --set-interpreter ${LDLINUX} ${i} 2>/dev/null
    done
  fi
done
echo ${EXTRACT}
set -e
. ${HERE}/lib/pack_toy.sh
pack_toy /tmp ${NAME}
mv -f /tmp/${NAME}.tar.xz ${HOME}
rm -rf ${EXTRACT}
echo END SCRIPT ${HOME}/${NAME}.tar.xz DONE

