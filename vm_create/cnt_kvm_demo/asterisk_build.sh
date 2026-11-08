#!/bin/bash
# asterisk_build.sh <asterisk.tar.gz> <pjproject.tar.bz2> <out.deb>, as root in
# a trixie holding the packages "asterisk_build.sh deps" prints: Asterisk
# compiled with the modules of MODULES only, the ones the demos load, and
# packed as one .deb whose Depends are the packages of the libraries it loads.
# Run by cnt_asterisk_trixie.sh in a build root, and by build_tools/vmbuild/
# builder.sh in the builder; the tarballs come from get_distro/get_asterisk.sh.
# wget: configure wants a download utility for the bundled pjproject even
# when the tarball is in the cache; nothing is downloaded.
DEPS="build-essential,pkg-config,bzip2,patch,binutils,wget,libedit-dev"
DEPS+=",libjansson-dev,libxml2-dev,libsqlite3-dev,uuid-dev,libsrtp2-dev,libssl-dev"
# ldconfig is in /usr/sbin, not in the PATH a chroot inherits from a user
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
MODULES="chan_pjsip chan_iax2 chan_rtp pbx_config cdr_csv
res_pjsip res_pjsip_session res_pjsip_pubsub res_pjsip_authenticator_digest
res_pjsip_outbound_authenticator_digest res_pjsip_endpoint_identifier_user
res_pjsip_endpoint_identifier_ip res_pjsip_registrar
res_pjsip_outbound_registration res_pjsip_sdp_rtp res_pjsip_dtmf_info
res_pjsip_caller_id res_pjsip_rfc3326 res_pjsip_logger res_pjsip_mwi
res_pjsip_mwi_body_generator res_pjsip_refer res_pjsip_nat res_pjsip_notify
res_pjproject res_sorcery_config res_sorcery_memory res_sorcery_astdb
res_rtp_asterisk res_srtp res_rtp_multicast res_timing_timerfd
res_musiconhold res_parking res_clioriginate
app_dial app_playback app_echo app_senddtmf app_voicemail app_confbridge
app_queue app_mixmonitor app_page app_playtones app_milliwatt app_verbose
func_callerid func_channel func_jitterbuffer func_logic func_timeout
bridge_simple bridge_native_rtp bridge_softmix bridge_holding
bridge_builtin_features codec_ulaw codec_alaw codec_g722 codec_gsm
codec_resample format_gsm format_wav format_pcm"
if [ "$1" = "deps" ]; then
  echo "${DEPS}"
  exit 0
fi
if [ $# -ne 3 ] || [ ! -r "$1" ] || [ ! -r "$2" ]; then
  echo "ERROR: asterisk_build.sh <asterisk.tar.gz> <pjproject.tar.bz2> <out.deb>"
  exit 1
fi
if [ ! -c /dev/null ]; then
  echo "ERROR: /dev/null is not a device here: configure scripts read back what"
  echo "       they wrote to it. The build root needs the /dev of the host bound."
  exit 1
fi
W=$(mktemp -d)
step() {
  echo "  asterisk: $1"
  if ! bash -c "$2" > ${W}/step.log 2>&1; then
    echo "ERROR asterisk: $1 failed, the end of its output:"
    tail -n 40 ${W}/step.log
    exit 1
  fi
}
tar -C ${W} -xzf "$1" || { echo "ERROR: $1 does not unpack"; exit 1; }
SRC=$(ls -d ${W}/asterisk-*)
[ -d "${SRC}" ] || { echo "ERROR: no asterisk-<version> in $1"; exit 1; }
. ${SRC}/third-party/versions.mak
# the bundled pjproject from the cache, checked by its md5 of the tarball:
# configure downloads nothing
mkdir -p ${W}/cache ${W}/pkg
cp "$2" ${W}/cache/pjproject-${PJPROJECT_VERSION}.tar.bz2
cd ${SRC}
step "configure" "./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
  --libdir=/usr/lib --with-pjproject-bundled --with-externals-cache=${W}/cache \
  --without-libjwt --disable-xmldoc"
# every category of modules, tools and sounds off, then MODULES on; the
# compile options keep their defaults (OPTIONAL_API: without it res_pjsip
# links to res_statsd and res_geolocation), but BUILD_NATIVE: the binaries
# run on other machines than the builder.
SEL="--disable BUILD_NATIVE"
for c in ADDONS APPS BRIDGES CDR CEL CHANNELS CODECS FORMATS FUNCS PBX RES \
         TESTS UTILS AGIS CORE_SOUNDS MOH EXTRA_SOUNDS; do
  SEL="${SEL} --disable-category MENUSELECT_${c}"
done
for m in ${MODULES}; do
  SEL="${SEL} --enable ${m}"
done
step "menuselect" "make menuselect.makeopts && \
  menuselect/menuselect ${SEL} menuselect.makeopts"
if ! grep -q '^MENUSELECT_CFLAGS=.*OPTIONAL_API' menuselect.makeopts; then
  echo "ERROR asterisk: OPTIONAL_API is off, res_pjsip would not load"
  exit 1
fi
# a module of ours still listed as disabled, or failing its dependencies, stops
CATS="ADDONS|APPS|BRIDGES|CDR|CEL|CHANNELS|CODECS|FORMATS|FUNCS|PBX|RES|TESTS"
for m in ${MODULES}; do
  if grep -E "^MENUSELECT_(${CATS})=" menuselect.makeopts | grep -qw "${m}"; then
    echo "ERROR asterisk: ${m} is not selected by menuselect"
    exit 1
  fi
  if grep -E '^MENUSELECT_DEPSFAILED=' menuselect.makeopts | grep -qE "=${m}\$"; then
    echo "ERROR asterisk: ${m} cannot be built, its dependencies are missing"
    exit 1
  fi
done
step "make" "make -j$(nproc) ASTDATADIR=/usr/share/asterisk"
step "install" "make install DESTDIR=${W}/pkg ASTDATADIR=/usr/share/asterisk"
VERSION=$(cat .version)
printf '\177ELF' > ${W}/magic
for f in $(find ${W}/pkg -type f); do
  if head -c 4 ${f} | cmp -s - ${W}/magic; then
    strip --strip-unneeded ${f} || { echo "ERROR: strip ${f}"; exit 1; }
    objdump -p ${f} | awk '$1 == "NEEDED" {print $2}' >> ${W}/needed
  fi
done
PKGS=""
for l in $(sort -u ${W}/needed | grep -v '^libasterisk'); do
  p=$(ldconfig -p | awk -v l="${l}" '$1 == l {print $NF; exit}')
  [ -n "${p}" ] || { echo "ERROR: ${l} needed by asterisk is in no library path"; exit 1; }
  owner=$(dpkg -S "${p}" 2>/dev/null || dpkg -S "$(readlink -f ${p})" 2>/dev/null)
  [ -n "${owner}" ] || { echo "ERROR: ${p} needed by asterisk belongs to no package"; exit 1; }
  PKGS="${PKGS} ${owner%%:*}"
done
DEPENDS=$(echo ${PKGS} | tr ' ' '\n' | sort -u | paste -sd, | sed 's/,/, /g')
mkdir -p ${W}/pkg/DEBIAN
cat > ${W}/pkg/DEBIAN/control << EOF
Package: asterisk
Version: ${VERSION}-cloonix
Architecture: amd64
Maintainer: cloonix <clownix@clownix.net>
Depends: ${DEPENDS}
Section: comm
Priority: optional
Description: Asterisk PBX ${VERSION}, the modules of the cloonix demos only
 Compiled by vm_create/cnt_kvm_demo/asterisk_build.sh from the asterisk tree
 of the local mirror, Debian trixie carrying no asterisk.
EOF
step "dpkg-deb" "dpkg-deb --root-owner-group --build ${W}/pkg $3"
echo "  asterisk ${VERSION}: $(du -h $3 | cut -f1), depends on ${DEPENDS}"
rm -rf ${W}
