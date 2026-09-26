#!/bin/bash
# Build the freebsd base VM image: ${BULK}/freebsd.qcow2 THIS ONE FETCHES FROM
# THE INTERNET, and it is the only builder here that does.
set -e
set -u
trap 'rc=$?; echo ""; \
      echo "FATAL: ${0##*/} stopped at line ${LINENO}, status ${rc}." >&2; \
      echo "       Nothing above says so, because the command that" >&2; \
      echo "       failed printed nothing. That line is the place." >&2' ERR
HERE=$(cd "$(dirname "$0")" && pwd)
BULK="/var/lib/cloonix/bulk"
DISTRO="freebsd"
QCOW2="${BULK}/${DISTRO}.qcow2"
REL="15.1-RELEASE"
# disc1, not bootonly: since 15.0 it carries the base system as an offline pkg
# repo (/usr/freebsd-packages), no base.txz. Only qemu-guest-agent needs the network.
ISO_NAME="FreeBSD-${REL}-amd64-disc1.iso"
ISO_URL="https://download.freebsd.org/releases/amd64/amd64/ISO-IMAGES/${REL%%-RELEASE}/${ISO_NAME}"
# Virtual size only: the qcow2 grows with what is written, and what an install
# writes is a couple of gigabytes.
DISK_SIZE_G=32
WORK="/tmp/kvm_freebsd_work"
LOCKFILE=/var/lib/cloonix/bulk
mkdir -p ${LOCKFILE} 2>/dev/null; exec 9<${LOCKFILE}
if ! flock -n 9; then
  echo "ERROR: another cloonix image build is already running."
  echo "       (${LOCKFILE} is held)"
  exit 1
fi
if [ "$(id -u)" = "0" ]; then
  echo "FATAL: do not run this as root."
  exit 1
fi
[ -d "${BULK}" ] || { echo "FATAL: ${BULK} does not exist"; exit 1; }
[ -w "${BULK}" ] || { echo "FATAL: ${BULK} not writable by $(id -un)"; exit 1; }
for t in wget xz qemu-img; do
  command -v "$t" >/dev/null 2>&1 || { echo "FATAL: $t not found"; exit 1; }
done
[ -x "${HERE}/freebsd_console.py" ] || {
  echo "FATAL: ${HERE}/freebsd_console.py is missing or not executable"
  exit 1; }
# The cloonix qemu, not the host's.
QEMU_BIN="/usr/libexec/cloonix/cloonfs/bin/cloonix-qemu-system"
[ -x "${QEMU_BIN}" ] || {
  echo "FATAL: ${QEMU_BIN} not found."
  echo "       This builder boots the image to configure it, so cloonix"
  echo "       has to be installed before freebsd can be built."
  exit 1; }
[ -w /dev/kvm ] || { echo "FATAL: /dev/kvm not writable by $(id -un)"; exit 1; }
PASST_BIN="/usr/libexec/cloonix/cloonfs/bin/cloonix-passt"
[ -x "${PASST_BIN}" ] || {
  echo "FATAL: ${PASST_BIN} not found."
  echo "       This is the one image here that needs the internet, and"
  echo "       passt is how cloonix gives a VM the internet."
  exit 1; }
# Built --without-default-devices: every device is named explicitly.
QEMU_DIR="/usr/libexec/cloonix/cloonfs/bin/qemu"
NETDEVS=$(${QEMU_BIN} -L ${QEMU_DIR} -machine q35 -netdev help 2>&1 || true)
if echo "${NETDEVS}" | grep -qw stream; then
  :
elif echo "${NETDEVS}" | grep -qi "netdev backend"; then
  echo "FATAL: ${QEMU_BIN} has no \"stream\" netdev."
  echo "       That is the backend passt presents, so without it there is"
  echo "       no way to give this guest the package mirror it needs."
  echo "       What this qemu does have:"
  echo "${NETDEVS}" | sed 's/^/         /'
  exit 1
else
  echo "note: could not read the netdev list from this qemu, so whether it"
  echo "      has the stream backend is not known here. Going on: the boot"
  echo "      below says so plainly if it has not. What the probe said:"
  echo "${NETDEVS}" | sed 's/^/        /'
fi
echo "======================================================================"
echo " freebsd ${REL}"
echo "   from   : ${ISO_URL}"
echo "   into   : ${QCOW2}"
echo "   NOTE   : this builder uses the INTERNET, unlike every other one"
echo "            here, and reaches it through cloonix-passt. That is"
echo "            why doitall.sh does not run it."
echo "======================================================================"
mkdir -p ${WORK}
if [ ! -s "${WORK}/${ISO_NAME}" ]; then
  echo "fetching ${ISO_NAME}"
  rm -f "${WORK}/${ISO_NAME}.part"
  wget -O "${WORK}/${ISO_NAME}.part" "${ISO_URL}"
  mv "${WORK}/${ISO_NAME}.part" "${WORK}/${ISO_NAME}"
else
  echo "already fetched: ${WORK}/${ISO_NAME}"
fi
[ -s "${WORK}/${ISO_NAME}" ] || {
  echo "FATAL: ${WORK}/${ISO_NAME} was not written"; exit 1; }
cat > "${WORK}/installerconfig" << 'INSTALLEREOF'
PARTITIONS=da0
# No DISTRIBUTIONS: bsdinstall then installs from packages, and from the media.
export BSDINSTALL_PKG_REPOS_DIR=/usr/freebsd-packages/repos/
# What base.txz was. Unset, kernel-dbg comes too.
export COMPONENTS="base"

#!/bin/sh
set -x

# Serial console for every boot of the installed system.
echo '-S115200 -Dh' > /boot.config

cat > /boot/loader.conf << EOT
boot_multicons="YES"
boot_serial="YES"
comconsole_speed="115200"
console="comconsole,vidconsole"
autoboot_delay="1"
# virtio_console is a module not in GENERIC: without it no /dev/vtcon, no agent
# channel, red node. The other virtio drivers are in GENERIC already.
virtio_console_load="YES"
EOT

cat > /etc/rc.conf << 'EOT'
hostname="freebsd"
# qemu-ga bootstraps the cloonix agent: it writes and runs /agent_cloonix_launcher.
qemu_guest_agent_enable="YES"
# -p names the channel: qemu-ga's default /dev/virtio-ports/<name> is the linux
# path. freebsd makes /dev/vtcon/<name>, often missing; the raw device is
# /dev/ttyV0.2 (the agent port is second on the qemu line). Both are tried,
# the log below says which opened. The name matches qemu_automaton.c.
qemu_guest_agent_dev="/dev/vtcon/org.qemu.guest_agent.0"
if [ ! -e "${qemu_guest_agent_dev}" ]; then
  qemu_guest_agent_dev="/dev/ttyV0.2"
fi
qemu_guest_agent_flags="-d -v -l /var/log/qemu-ga.log -p ${qemu_guest_agent_dev}"
sshd_enable="YES"
# No growfs (nothing to grow, and a first-boot rewrite once left an unfsckable
# root); no ifconfig_ line (a dhcp client with no server holds up the boot).
EOT

# The same two accounts the linux images here carry, so a demo written
# against one works against the other.
echo root | pw usermod root -h 0
pw useradd -n user -m -s /bin/sh 2>/dev/null || true
echo user | pw usermod user -h 0
sed -i '' -e 's/^#*PermitRootLogin.*/PermitRootLogin yes/' \
    /etc/ssh/sshd_config
sed -i '' -e 's/^#*PermitEmptyPasswords.*/PermitEmptyPasswords no/' \
    /etc/ssh/sshd_config

# Where the agent disc gets mounted by the launcher qga runs.
mkdir -p /cloonixmnt/cnf_fs /cloonixmnt/tmp

# The one step needing the internet; the address is removed below.
dhclient vtnet0 || true
echo "nameserver CLOONIX_DNS" > /etc/resolv.conf
# pkg itself came with the base packages.
env ASSUME_ALWAYS_YES=YES pkg install -y qemu-guest-agent
# Checked: pkg exits 0 having installed nothing when the package is there under another name.
if [ ! -x /usr/local/sbin/qemu-ga ] && [ ! -x /usr/local/bin/qemu-ga ]; then
  echo "CLOONIX_INSTALL_NO_QEMU_GA"
  poweroff
fi
# The base packages the install copied off the media.
pkg clean -ay || true
killall dhclient 2>/dev/null || true
rm -f /var/db/dhclient.leases.vtnet0 /etc/resolv.conf

# The marker freebsd_console.py waits for. Without it a poweroff and a
# crash at the last step look the same from the host.
echo "CLOONIX_INSTALL_DONE"
sync
poweroff
INSTALLEREOF
CLOONIX_DNS=""
for f in /run/systemd/resolve/resolv.conf /etc/resolv.conf; do
  [ -r "${f}" ] || continue
  CLOONIX_DNS=$(awk '/^nameserver/ && $2 !~ /^127\./ {print $2; exit}' \
                "${f}" 2>/dev/null || true)
  [ -n "${CLOONIX_DNS}" ] && break
done
[ -n "${CLOONIX_DNS}" ] || CLOONIX_DNS="9.9.9.9"
sed -i "s/CLOONIX_DNS/${CLOONIX_DNS}/" "${WORK}/installerconfig"
echo "install-time resolver: ${CLOONIX_DNS}"

cat > "${WORK}/loader.conf.local" << 'LOADEREOF'
# The install is watched over the serial line: loader and kernel speak there.
boot_multicons="YES"
boot_serial="YES"
comconsole_speed="115200"
console="comconsole,vidconsole"
autoboot_delay="1"
LOADEREOF
command -v xorriso >/dev/null 2>&1 || {
  echo "FATAL: xorriso not found, and the install media has to be rebuilt"
  echo "       to carry /etc/installerconfig."
  echo "         apt install xorriso"
  exit 1; }
rm -f "${WORK}/install.iso"
# Takes minutes and says nothing while running.
echo "building the install media: rewriting $(du -h "${WORK}/${ISO_NAME}" \
      | cut -f1) of iso to add /etc/installerconfig."
echo "  this takes a few minutes and prints nothing while it runs"
ISO_LDSIZ=$(xorriso -indev "${WORK}/${ISO_NAME}" -report_el_torito plain \
            2>/dev/null | awk '/boot img/{print $(NF-1); exit}' || true)
case "${ISO_LDSIZ}" in
  ''|*[!0-9]*)
    echo "FATAL: no el torito load size could be read from ${ISO_NAME}."
    echo "       Guessing it is what makes a disc that loads a fraction"
    echo "       of its own boot loader and resets, so this stops here."
    exit 1 ;;
esac
ISO_LOAD_BYTES=$((ISO_LDSIZ * 512))
echo "boot image: ${ISO_LDSIZ} sectors, ${ISO_LOAD_BYTES} bytes"
if ! xorriso -indev "${WORK}/${ISO_NAME}" -find /boot/cdboot \
     2>/dev/null | grep -q cdboot; then
  echo "FATAL: /boot/cdboot is not on ${ISO_NAME}."
  echo "       That is the boot image the rebuilt media is given, and"
  echo "       without it the install disc would not start. Either the"
  echo "       download is partial or this release lays its boot code"
  echo "       out somewhere new."
  exit 1
fi
if ! xorriso -indev "${WORK}/${ISO_NAME}" \
             -outdev "${WORK}/install.iso" \
             -map "${WORK}/installerconfig" /etc/installerconfig \
             -map "${WORK}/loader.conf.local" /boot/loader.conf.local \
             -boot_image any discard \
             -boot_image any cat_path=/boot.catalog \
             -boot_image any bin_path=/boot/cdboot \
             -boot_image any emul_type=no_emulation \
             -boot_image any load_size=${ISO_LOAD_BYTES} \
             > "${WORK}/xorriso.log" 2>&1; then
  echo "FATAL: the install media could not be rebuilt. xorriso said:"
  tail -20 "${WORK}/xorriso.log" | sed 's/^/    /'
  exit 1
fi
[ -s "${WORK}/install.iso" ] || {
  echo "FATAL: the install media could not be rebuilt, and xorriso"
  echo "       reported no error. It said:"
  tail -20 "${WORK}/xorriso.log" | sed 's/^/    /'
  exit 1; }
NEW_LDSIZ=$(xorriso -indev "${WORK}/install.iso" -report_el_torito plain \
            2>/dev/null | awk '/boot img/{print $(NF-1); exit}' || true)
if [ -z "${NEW_LDSIZ}" ]; then
  echo "FATAL: the rebuilt media has no el torito boot entry, so no bios"
  echo "       would start it. xorriso said:"
  tail -20 "${WORK}/xorriso.log" | sed 's/^/    /'
  exit 1
fi
# Compared with the source, not merely present.
if [ "${NEW_LDSIZ}" != "${ISO_LDSIZ}" ]; then
  echo "FATAL: the rebuilt media loads ${NEW_LDSIZ} sectors of boot code"
  echo "       where ${ISO_NAME} loads ${ISO_LDSIZ}."
  echo "       A bios given the smaller figure loads part of the loader,"
  echo "       jumps into it and the guest resets -- silently, because"
  echo "       qemu exits on that reset with nothing printed anywhere."
  exit 1
fi
ISO_HAS=$(xorriso -indev "${WORK}/install.iso" -find / 2>/dev/null || true)
for i in /etc/installerconfig /boot/loader.conf.local \
         /usr/freebsd-packages/repos/FreeBSD-base-offline.conf \
         /usr/freebsd-packages/offline; do
  if ! echo "${ISO_HAS}" | grep -q "'${i}'"; then
    echo "FATAL: ${i} is not on the rebuilt media."
    case "${i}" in
      /etc/*)
        echo "       Without it bsdinstall asks its first question and"
        echo "       waits for an answer nothing here will give." ;;
      /boot/*)
        echo "       Without it the install media talks to the video"
        echo "       console, which this build does not give the guest,"
        echo "       and the whole install runs invisibly." ;;
      *)
        echo "       That is the offline package repo the base system is"
        echo "       installed from, carried by disc1 since 15.0. Either"
        echo "       ${WORK}/${ISO_NAME}"
        echo "       is a partial download (remove it and run again), or"
        echo "       REL is older than 15.0 or this release moved the repo."
        echo "       What the media has under /usr/freebsd-packages:"
        echo "${ISO_HAS}" | grep "'/usr/freebsd-packages" | head -8 \
          | sed 's/^/         /' ;;
    esac
    exit 1
  fi
done
echo "install media: ${WORK}/install.iso  $(du -h "${WORK}/install.iso" \
      | cut -f1)"
BUILDING="${QCOW2}.building"
rm -f "${BUILDING}"
qemu-img create -f qcow2 "${BUILDING}" ${DISK_SIZE_G}G >/dev/null
echo
echo "installing (this takes a while and prints as it goes)"
echo
if ! "${HERE}/freebsd_console.py" "${WORK}/install.iso" "${BUILDING}"; then
  echo
  echo "======================================================================"
  echo " FAILED: the install did not finish."
  echo
  echo " ${QCOW2}"
  echo " has NOT been written, so nothing here can be booted by mistake."
  echo " The unfinished disk is ${BUILDING},"
  echo " kept only to be looked at, and the whole session is in"
  echo " /tmp/freebsd_console.log."
  echo "======================================================================"
  exit 1
fi
echo "compacting (O_DIRECT and niced, so the live nets keep their beats)"
IONICE=""
command -v ionice >/dev/null 2>&1 && IONICE="ionice -c3"
# The cache flags are asked for, not assumed.
CACHEOPT=""
if qemu-img convert --help 2>&1 | grep -q '^\s*-t '; then
  CACHEOPT="-t none"
fi
if qemu-img convert --help 2>&1 | grep -q '^\s*-T '; then
  CACHEOPT="${CACHEOPT} -T none"
fi
if [ -z "${CACHEOPT}" ]; then
  echo "  note: this qemu-img has no -t/-T, so the convert goes through"
  echo "        the page cache. If a c2c drops here, that is why."
fi
nice -n 19 ${IONICE} \
  qemu-img convert ${CACHEOPT} -O qcow2 "${BUILDING}" "${QCOW2}.tmp"
rm -f "${BUILDING}"
# The last thing that happens, and the only thing that creates the name "add
# freebsd" looks for.
mv -f "${QCOW2}.tmp" "${QCOW2}"
echo
echo "======================================================================"
echo " DONE ${QCOW2}   $(du -sh ${QCOW2} | cut -f1)"
echo
echo "cloonix cli <net> add freebsd bsd1 ram=2000 cpu=2 eth=1 freebsd.qcow2"
echo "======================================================================"
