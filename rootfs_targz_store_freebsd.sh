#!/bin/bash
# rootfs_targz_store_freebsd.sh Puts ONE tarball in targz_store: the freebsd
# headers and static libraries the insider agents are cross compiled against.
set -e
trap 'rc=$?; echo ""; \
      echo "FATAL: ${0##*/} stopped at line ${LINENO}, status ${rc}." >&2; \
      echo "       The command that failed printed nothing; that line" >&2; \
      echo "       is the place to look." >&2' ERR
HERE=$(cd "$(dirname "$0")" && pwd)
TARGZ="${HERE}/targz_store"
WORK="${HERE}/work_targz_store/freebsd_sysroot"
DL="/tmp/cloonix_freebsd_base"
FBSD_REL="15.1-RELEASE"
FBSD_MAJ="15"
FBSD_URL="https://download.freebsd.org/releases/amd64/${FBSD_REL}/base.txz"
NAME="freebsd_sysroot_${FBSD_REL}"
LIBS="libc.a libutil.a"
LIBS="${LIBS} libgcc.a libgcc_eh.a libcompiler_rt.a"
LIBS="${LIBS} libssp_nonshared.a libm.a libthr.a"
# The startup objects, taken as a GLOB and not as a list.
case "${1:-}" in
  ""|--check|-check) : ;;
  -h|--help|-help|help)
    sed -n '3,12p' "$0" | sed 's/^# \?//'
    exit 0 ;;
  *)
    echo "unknown option: $1" >&2
    echo "  (no argument)   fetch and make the sysroot tarball" >&2
    echo "  --check         say what the store holds, fetch nothing" >&2
    exit 1 ;;
esac
if [ "${1:-}" = "--check" ] || [ "${1:-}" = "-check" ]; then
  TGZ=$(ls "${TARGZ}"/freebsd_sysroot_*.tar.gz 2>/dev/null | head -n1)
  if [ -z "${TGZ}" ]; then
    echo "NO freebsd_sysroot_*.tar.gz in ${TARGZ}"
    echo "  run this script with no argument to make it"
    exit 1
  fi
  echo "store: ${TGZ}"
  HELD=$(tar tzf "${TGZ}" 2>/dev/null)
  RC=0
  for i in usr/include/stdio.h usr/lib/libc.a usr/lib/libutil.a \
           usr/lib/libgcc.a usr/lib/crt1.o usr/lib/crti.o usr/lib/crtn.o \
           usr/lib/crtbeginT.o usr/lib/crtend.o; do
    if echo "${HELD}" | grep -q "/${i}$"; then
      printf "  ok      %s\n" "${i}"
    else
      printf "  MISSING %s\n" "${i}"
      RC=1
    fi
  done
  if [ ${RC} -eq 0 ]; then
    echo "this store tarball can link the freebsd agents"
  else
    echo ""
    echo "this store tarball CANNOT link the freebsd agents: it was made"
    echo "by an older version of this script. Run it with no argument."
  fi
  exit ${RC}
fi
for t in wget tar; do
  command -v "$t" >/dev/null 2>&1 || { echo "FATAL: $t not found"; exit 1; }
done
mkdir -p "${TARGZ}"
rm -f "${TARGZ}"/freebsd_sysroot_*.tar.gz
rm -rf "${WORK}"
mkdir -p "${WORK}/${NAME}"
echo "======================================================================"
echo " freebsd sysroot ${FBSD_REL}"
echo "   from  : ${FBSD_URL}"
echo "   into  : ${TARGZ}/${NAME}.tar.gz"
echo "   The download is about 200M, kept in ${DL}"
echo "   so a rerun does not fetch it again. What goes in the store is a"
echo "   few megabytes, and ./doitall then needs no network for it."
echo "======================================================================"
mkdir -p "${DL}"
if [ ! -s "${DL}/base_${FBSD_REL}.txz" ]; then
  wget -O "${DL}/base_${FBSD_REL}.txz.part" "${FBSD_URL}"
  mv "${DL}/base_${FBSD_REL}.txz.part" "${DL}/base_${FBSD_REL}.txz"
else
  echo "already fetched: ${DL}/base_${FBSD_REL}.txz"
fi
ln -sf "${DL}/base_${FBSD_REL}.txz" "${WORK}/base.txz"
set +e
tar --directory="${WORK}/${NAME}" -xf "${WORK}/base.txz" \
    --wildcards --exclude './usr/include/c++/*' './usr/include/*'
TAR_RC=$?
for i in ${LIBS}; do
  tar --directory="${WORK}/${NAME}" -xf "${WORK}/base.txz" \
      "./usr/lib/${i}" 2>/dev/null
done
tar --directory="${WORK}/${NAME}" -xf "${WORK}/base.txz" \
    --wildcards './usr/lib/*.o' 2>/dev/null
set -e
echo "x86_64-unknown-freebsd${FBSD_MAJ}" > "${WORK}/${NAME}/CLOONIX_TARGET"
echo "${FBSD_REL}" > "${WORK}/${NAME}/CLOONIX_RELEASE"
rm -f "${WORK}/base.txz"   # the symlink only, the cache stays
# Checked here, on the extracted tree, and not left to the link that would
# fail later.
MISSING=""
for i in usr/include/stdio.h \
         usr/include/sys/sysctl.h \
         usr/include/libutil.h \
         usr/lib/libc.a \
         usr/lib/libutil.a \
         usr/lib/libgcc.a \
         usr/lib/crt1.o \
         usr/lib/crti.o \
         usr/lib/crtbeginT.o \
         usr/lib/crtend.o; do
  [ -e "${WORK}/${NAME}/${i}" ] || MISSING="${MISSING} ${i}"
done
if [ -n "${MISSING}" ] || [ "${TAR_RC}" != "0" ]; then
  echo "##########################################################" >&2
  echo "THE SYSROOT IS INCOMPLETE" >&2
  if [ "${TAR_RC}" != "0" ]; then
    echo "  tar exited ${TAR_RC} on the headers" >&2
  fi
  for i in ${MISSING}; do echo "    missing: ${i}" >&2; done
  echo "" >&2
  echo "base.txz did not hold what a static cross link needs, or the" >&2
  echo "layout has moved and the patterns in this script no longer" >&2
  echo "match it. Nothing is written to targz_store: a tarball that" >&2
  echo "compiles and does not link is worse than none." >&2
  echo "##########################################################" >&2
  rm -rf "${WORK}"
  exit 1
fi
cd "${WORK}"
tar zcf "${NAME}.tar.gz" "${NAME}"
rm -f "${TARGZ}"/freebsd_sysroot_*.tar.gz
mv "${NAME}.tar.gz" "${TARGZ}"
cd "${HERE}"
rm -rf "${WORK}"
rmdir "${HERE}/work_targz_store" 2>/dev/null || true
echo
echo "======================================================================"
echo " DONE  ${TARGZ}/${NAME}.tar.gz   $(du -h ${TARGZ}/${NAME}.tar.gz | cut -f1)"
echo
echo " the startup objects it holds, which is what a static link opens:"
tar tzf "${TARGZ}/${NAME}.tar.gz" | grep '/usr/lib/.*\.o$' \
  | sed 's|.*/usr/lib/|   |'
echo
echo " ./rootfs_targz_store_freebsd.sh --check   says this again later"
echo
echo " ./doitall reads it from there and downloads nothing."
echo "======================================================================"
