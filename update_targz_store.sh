#!/bin/bash
# Refill targz_store, the sources every cloonix build reads.
HERE=$(cd "$(dirname "$0")" && pwd)
TARGZ=${HERE}/targz_store
WORK=${HERE}/work_targz_store

ALL="passt crun patchelf qemu ovs"

get_passt() {
cd ${WORK}
git clone --depth=1 https://passt.top/passt
cd ${WORK}/passt
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf passt_${COMMIT}.tar.gz passt
rm -rf passt
mv passt_${COMMIT}.tar.gz ${TARGZ}
}
get_crun() {
cd ${WORK}
git clone --depth=1 https://github.com/containers/crun.git
cd ${WORK}/crun
COMMIT=$(git log --pretty=format:"%H")
./autogen.sh
./configure
cd ${WORK}
tar zcvf crun_${COMMIT}.tar.gz crun
rm -rf crun
mv crun_${COMMIT}.tar.gz ${TARGZ}
}
get_patchelf() {
cd ${WORK}
git clone --depth=1 https://github.com/NixOS/patchelf.git
cd ${WORK}/patchelf
COMMIT=$(git log --pretty=format:"%H")
./bootstrap.sh
./configure
cd ${WORK}
tar zcvf patchelf_${COMMIT}.tar.gz patchelf 
rm -rf patchelf
mv patchelf_${COMMIT}.tar.gz ${TARGZ}
}
get_qemu() {
cd ${WORK}
git clone --depth=1 https://git.qemu.org/git/qemu.git
cd ${WORK}/qemu
COMMIT=$(git log --pretty=format:"%H")
git submodule init
git submodule update --recursive
./scripts/archive-source.sh ${WORK}/qemu.tar.gz
cd ${WORK}
mkdir qemu_vip
cd ${WORK}/qemu_vip
tar xvf ${WORK}/qemu.tar.gz
meson subprojects download
cd ${WORK}
tar zcvf qemu_${COMMIT}.tar.gz qemu_vip
rm -rf qemu
rm -rf qemu_vip
rm -f qemu.tar.gz
mv qemu_${COMMIT}.tar.gz ${TARGZ}
}
get_ovs() {
cd ${WORK}
git clone --depth=1 https://github.com/openvswitch/ovs.git
cd ${WORK}/ovs
COMMIT=$(git log --pretty=format:"%H")
cd ${WORK}
tar zcvf ovs_${COMMIT}.tar.gz ovs
rm -rf ovs
mv ovs_${COMMIT}.tar.gz ${TARGZ}
}
case "${1:-}" in
  --list|-list|list)
    echo "${ALL}"
    exit 0 ;;
  -h|--help|-help|help)
    sed -n '3,14p' "$0" | sed 's/^# \?//'
    exit 0 ;;
esac
WANTED="$*"
if [ -z "${WANTED}" ]; then
  WANTED="${ALL}"
fi
for w in ${WANTED}; do
  case " ${ALL} " in
    *" ${w} "*) : ;;
    *) echo "unknown source: ${w}"
       echo "known: ${ALL}"
       exit 1 ;;
  esac
done
mkdir -vp ${TARGZ}
mkdir -vp ${WORK}
echo "the store is ${TARGZ}"
for w in ${WANTED}; do
  echo "-------------------------------------------------------------------"
  echo " ${w}"
  echo "-------------------------------------------------------------------"
  get_${w} || { echo "FAILED: ${w}" >&2; exit 1; }
done
cd ${HERE}
rmdir work_targz_store 2>/dev/null
echo
echo "in ${TARGZ}:"
ls -1 ${TARGZ}
