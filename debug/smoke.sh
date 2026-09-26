#!/bin/bash
# smoke.sh - the ten minutes that would have saved the evening.
NET=${1:-nemo}
KVM_ROOTFS=${2:-}
CNT_ROOTFS=${3:-}
KVM=smokekvm
CNT=smokecnt
FAILED=0
STEP=""

CLOONIX=$(command -v cloonix || echo /usr/libexec/cloonix/cloonfs/bin/cloonix)
WORK=/var/lib/cloonix/${NET}

step()  { STEP="$1"; printf "%-58s" "  ${STEP}"; }
ok()    { echo "ok"; }
ko()    { echo "FAILED"; [ -n "$1" ] && echo "      $1"; FAILED=1; }
# Run a command inside a guest and return what it printed.
guest_say()
{
  local name="$1" mark="$2"
  timeout 25 ${CLOONIX} cli ${NET} ssh ${name} "echo ${mark}" 2>/dev/null
}

echo
echo "cloonix smoke test on net ${NET}"
echo

step "wire constants agree"
if $(dirname $0)/check_wire.sh >/dev/null 2>&1; then ok; else
  ko "run debug/check_wire.sh to see the difference"; fi

step "docs name only delivered binaries"
if $(dirname $0)/check_docs.sh >/dev/null 2>&1; then ok; else
  ko "run debug/check_docs.sh"; fi

step "the tree spells the command as the client reads it"
if $(dirname $0)/check_cli.py >/dev/null 2>&1; then ok; else
  ko "run debug/check_cli.py"; fi

step "net starts"
${CLOONIX} start ${NET} >/dev/null 2>&1
sleep 3
if ${CLOONIX} cli ${NET} dump commands >/dev/null 2>&1; then ok; else
  ko "the net did not come up, nothing below can run"
  exit 1
fi

if [ -n "${KVM_ROOTFS}" ]; then
  step "kvm guest boots"
  ${CLOONIX} cli ${NET} add kvm ${KVM} ram=2000 cpu=2 eth=1 \
             ${KVM_ROOTFS} >/dev/null 2>&1
  for i in $(seq 1 60); do
    guest_say ${KVM} SMOKE_KVM 2>/dev/null | grep -q SMOKE_KVM && break
    sleep 2
  done
  if guest_say ${KVM} SMOKE_KVM | grep -q SMOKE_KVM; then ok; else
    ko "no shell in the kvm after two minutes"; fi
else
  echo "  (no kvm rootfs given, skipping the kvm steps)"
fi

if [ -n "${CNT_ROOTFS}" ]; then
  step "container starts"
  ${CLOONIX} cli ${NET} add cnt ${CNT} eth=1 ${CNT_ROOTFS} >/dev/null 2>&1
  sleep 5
  if ${CLOONIX} cli ${NET} dump commands 2>/dev/null | grep -q ${CNT}; then ok; else
    ko "the container is not in the topology"; fi

  step "container console reaches a shell"
  if guest_say ${CNT} SMOKE_CNT | grep -q SMOKE_CNT; then ok; else
    ko "the cxp leg into the container does not answer"; fi

  step "container console socket belongs to the caller"
  SOCK=${WORK}/dtach/cnt_${CNT}
  # Opening it is what the gui server does; root ownership is what broke it.
  if [ -S "${SOCK}" ]; then
    OWNER=$(stat -c %U "${SOCK}")
    if [ "${OWNER}" = "$(id -un)" ]; then ok; else
      ko "${SOCK} is owned by ${OWNER}, the gui server cannot open it"; fi
  else
    echo "skipped (no session open)"
  fi
else
  echo "  (no cnt rootfs given, skipping the container steps)"
fi

if [ -n "${KVM_ROOTFS}" ]; then
  step "qemu console socket exists"
  if [ -S "${WORK}/dtach/${KVM}" ]; then ok; else
    ko "${WORK}/dtach/${KVM} is not a socket"; fi

  step "scp puts a file and gets it back"
  echo "smoke_$$" > /tmp/smoke_put.txt
  rm -f /tmp/smoke_get.txt
  ${CLOONIX} cli ${NET} scp /tmp/smoke_put.txt ${KVM}:/tmp/ >/dev/null 2>&1
  ${CLOONIX} cli ${NET} scp ${KVM}:/tmp/smoke_put.txt /tmp/smoke_get.txt \
             >/dev/null 2>&1
  if [ -f /tmp/smoke_get.txt ] && \
     [ "$(cat /tmp/smoke_put.txt)" = "$(cat /tmp/smoke_get.txt)" ]; then ok
  else
    ko "the file did not survive the round trip"; fi
fi

CFG=$(ls /usr/libexec/cloonix/cloonfs/etc/cloonix.cfg \
         /cloonix/cloonfs/etc/cloonix.cfg 2>/dev/null | head -1)
PASS=$(awk -v n="${NET}" '
  $1=="CLOONIX_NET:" && $2==n {f=1; next}
  f && $1=="cloonix_passwd" {print $2; exit}' ${CFG} 2>/dev/null)
PORT=$(awk -v n="${NET}" '
  $1=="CLOONIX_NET:" && $2==n {f=1; next}
  f && $1=="cloonix_web" {print $2; exit}' ${CFG} 2>/dev/null)

if [ -n "${PASS}" ] && [ -n "${PORT}" ] && command -v curl >/dev/null; then
  step "https refuses a wrong password"
  CODE=$(curl -sk -o /dev/null -w '%{http_code}' \
         -u "${NET}:definitely_not_the_password" \
         https://127.0.0.1:${PORT}/ 2>/dev/null)
  if [ "${CODE}" = "401" ]; then ok; else
    ko "expected 401, got ${CODE}"; fi

  step "https accepts the right password"
  CODE=$(curl -sk -o /dev/null -w '%{http_code}' -u "${NET}:${PASS}" \
         https://127.0.0.1:${PORT}/ 2>/dev/null)
  if [ "${CODE}" = "200" ]; then ok; else
    ko "expected 200, got ${CODE}. The gui server may have started with no
      password: look for 'no cloonix_passwd found' in syslog"; fi
else
  echo "  (no curl or no cloonix_web for ${NET}, skipping the https steps)"
fi

step "net goes down cleanly"
${CLOONIX} stop ${NET} >/dev/null 2>&1
sleep 5
LEFT=$(pgrep -f "cloonix-.*${NET}" 2>/dev/null | wc -l)
if [ "${LEFT}" = "0" ] && [ ! -d "${WORK}" ]; then ok; else
  ko "${LEFT} process(es) left, and ${WORK} $([ -d ${WORK} ] && echo remains)"
fi

echo
if [ ${FAILED} -eq 0 ]; then
  echo "SMOKE OK"
else
  echo "SMOKE FAILED -- do not ship this build"
fi
exit ${FAILED}
