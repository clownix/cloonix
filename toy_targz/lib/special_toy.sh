#!/bin/bash
# Sourced by create_<name>_toy_targz.sh (NAME, IMAGE, KEEP): the toy without
# kvm, its image merged into self_rootfs, KEEP demos and <name>_demo/.
HERE=$(cd "$(dirname "$0")" && pwd)
set -e
if [ -z "${NAME}" ] || [ -z "${IMAGE}" ] || [ ! -d ${HERE}/${NAME}_demo ]; then
  echo "lib/special_toy.sh is sourced by create_<name>_toy_targz.sh"
  exit 1
fi
AREA="${NAME}_demo"
SPEC_DEMO="${HERE}/${AREA}"
IMAGE_TARGZ="/var/lib/cloonix/bulk/${IMAGE}.tar.gz"
TOY_NAME="cloonix-${NAME}-toy"
WORK="/tmp/${TOY_NAME}_work_${USER}"
TOP="${WORK}/${TOY_NAME}"
CLOONFS="${TOP}/rootfs/cloonix/cloonfs"
DEMOS="${CLOONFS}/share/cloonix/demos"
AUDIO="${CLOONFS}/bin/jscloonix/audio"
if [ ! -d ${SPEC_DEMO} ] || [ -z "$(ls ${SPEC_DEMO}/*.py 2>/dev/null)" ]; then
  echo "no demo in ${SPEC_DEMO}"
  exit 1
fi
if [ ! -r ${IMAGE_TARGZ} ]; then
  echo "${IMAGE_TARGZ} not found, fetch it:"
  echo "cd /var/lib/cloonix/bulk"
  echo "wget https://clownix.net/downloads/cnt_demo_bulk/${IMAGE}.tar.gz"
  echo "or build it:"
  echo "cd vm_create/cnt_kvm_demo && ./cnt_${IMAGE}.sh"
  exit 1
fi
rm -rf ${WORK}
mkdir -p ${WORK}
# HOME redirected: the classic toy lands in WORK, not over ${HOME}'s.
cd ${HERE}
HOME=${WORK} ./create_toy_targz.sh > ${WORK}/create_toy_targz.log 2>&1 || {
  echo "create_toy_targz.sh failed, see ${WORK}/create_toy_targz.log"
  exit 1
}
TOY=$(ls ${WORK}/cloonix-toy.tar.xz)
tar -C ${WORK} -xpf ${TOY}
rm -f ${TOY}
mv ${WORK}/$(basename ${TOY} .tar.xz) ${TOP}

cd ${DEMOS}
for f in ${KEEP}; do
  if [ ! -e "${f}" ]; then
    echo "${f} missing in ${DEMOS}"
    exit 1
  fi
done
# all.py plays the demos of the list: kept, with its packages line
for f in $(ls *.py */*.py | grep -v "^lib/"); do
  case " ${KEEP} all.py " in
    *" ${f} "*) ;;
    *) rm -f "${f}" ;;
  esac
done
find . -mindepth 1 -maxdepth 1 -type d ! -name lib -empty -delete
# packages and voice: kept demos only, a package without demo goes.
awk -v keep="${KEEP} all.py" '
  BEGIN { n = split(keep, k, " "); for (i = 1; i <= n; i++) kept[k[i]] = 1 }
  /^#/ { print; next }
  /^[0-9]+[ \t]/ { pending = $0; next }
  ($1 in kept) { if (pending != "") { print ""; print pending; pending = "" }
                 print }' packages > packages.tmp
mv -f packages.tmp packages
awk -v keep="${KEEP}" '
  BEGIN { n = split(keep, k, " "); for (i = 1; i <= n; i++) kept[k[i]] = 1 }
  /^#/ { print; next }
  ($1 in kept) { if (!sep) { print ""; sep = 1 } print }' voice > voice.tmp
mv -f voice.tmp voice
# The demos of <name>_demo: the examples of the documentation of the project.
mkdir -p ${AREA}
cp -f ${SPEC_DEMO}/*.py ${AREA}
cp -f ${HERE}/lib/toylib.py ${SPEC_DEMO}/lib/*.py lib
grep -v "^#" ${SPEC_DEMO}/packages >> packages
grep -v "^#" ${SPEC_DEMO}/voice >> voice
chmod 755 ${AREA}
chmod 644 ${AREA}/*.py lib/*.py
chmod 644 packages voice
if [ -d ${AUDIO} ]; then
  cd ${AUDIO}
  # the clips of the kept demos, of all.py, and the one of a failure
  for f in $(find . -name "*.ogg" | sed 's%^\./%%'); do
    case " ${KEEP} all.py demo_failed.py " in
      *" ${f%.ogg}.py "*) ;;
      *) rm -f "${f}" ;;
    esac
  done
  find . -mindepth 1 -type d -empty -delete
fi
if [ -d ${SPEC_DEMO}/audio ]; then
  mkdir -p ${AUDIO}/${AREA}
  cp -f ${SPEC_DEMO}/audio/*.ogg ${AUDIO}/${AREA}
fi

# The demos run on self_rootfs, the root of the toy, which gets the image.
cd ${DEMOS}
for f in ${KEEP} ${AREA}/*.py; do
  sed -i "s/^ROOTFS_CHOICES = (\"${IMAGE}\",)\$/ROOTFS_CHOICES = (\"self_rootfs\",)/" ${f}
  if ! grep -q '^ROOTFS_CHOICES = ("self_rootfs",)$' ${f}; then
    echo "${f}: no ROOTFS_CHOICES = (\"${IMAGE}\",) to turn into self_rootfs"
    exit 1
  fi
done

ROOTFS="${TOP}/rootfs"
in_root() { unshare -rm chroot ${CLOONFS} "$@"; }
SIZE0=$(du -sk ${ROOTFS} | cut -f1)

# No kvm: qemu, its firmware, the discs, the vnc console, xorriso, and the
# libraries nothing else loads.
CAND=""
for b in bin/cloonix-qemu-system bin/xorriso; do
  if [ ! -e ${CLOONFS}/${b} ]; then
    echo "${CLOONFS}/${b} missing"
    exit 1
  fi
  CAND="${CAND} $(in_root /lib64/ld-linux-x86-64.so.2 --list /${b} | \
                  awk '$2 == "=>" {print $3}')"
done
rm -rf ${CLOONFS}/bin/cloonix-qemu-system ${CLOONFS}/bin/xorriso \
       ${CLOONFS}/bin/qemu ${CLOONFS}/bin/jscloonix/novnc \
       ${CLOONFS}/bin/jscloonix/cloonix-novnc.html \
       ${ROOTFS}/cloonix/insider_agents/*.iso
CAND=$(echo ${CAND} | tr ' ' '\n' | sort -u)
GONE=""
again=1
while [ ${again} = 1 ]; do
  again=0
  for l in ${CAND}; do
    if [ ! -e ${CLOONFS}${l} ]; then
      continue
    fi
    so=$(basename ${l})
    if ! grep -rqF --exclude=${so} "${so}" ${CLOONFS} ${ROOTFS}/cloonix/startup_bin; then
      rm -f ${CLOONFS}${l}
      GONE="${GONE} ${so}"
      again=1
    fi
  done
done
SIZE1=$(du -sk ${ROOTFS} | cut -f1)

# Of the image, only what the demos need (special_needs.py): the commands
# they run, the packages of ${AREA}/needs, the libraries of both; the toy
# root keeps its own files, the image accounts are added.
IMG="${WORK}/image"
mkdir -p ${IMG}
tar -C ${IMG} --no-same-owner --exclude=./dev -xzf ${IMAGE_TARGZ}
NEEDS="${WORK}/needs"
echo "${IMAGE}: the commands taken, and for what:"
PYMODS="${WORK}/pymods"
SPECIAL_NEEDS_PYMODS=${PYMODS} \
python3 ${HERE}/lib/special_needs.py ${IMG} ${SPEC_DEMO}/needs \
  $(cd ${DEMOS} && ls ${KEEP} ${AREA}/*.py lib/demolib.py lib/toylib.py \
                      $(cd ${SPEC_DEMO} && ls lib/*.py)) > ${NEEDS}
for f in passwd group shadow gshadow; do
  if [ -e ${IMG}/etc/${f} ] && [ -e ${CLOONFS}/etc/${f} ]; then
    awk -F: 'NR == FNR { have[$1] = 1; next } !($1 in have)' \
        ${CLOONFS}/etc/${f} ${IMG}/etc/${f} >> ${CLOONFS}/etc/${f}
  fi
done
ADDED="${WORK}/added"
while read -r p; do
  [ -e "${CLOONFS}/${p}" ] || [ -L "${CLOONFS}/${p}" ] || echo "${p}"
done < ${NEEDS} > ${ADDED}
NB_ADDED=$(wc -l < ${ADDED})
tar -C ${IMG} --no-recursion -cf - -T ${NEEDS} | \
  tar -C ${CLOONFS} --keep-directory-symlink --skip-old-files \
      --no-same-owner -xf -
rm -rf ${IMG}
# Every added executable or shared object must load in the toy root (an
# older kept library shows here); the elf's own directory is searched, as
# ld.so --list ignores a library's RUNPATH.
printf '\177ELF' > ${WORK}/elf_magic
BAD=$(cd ${CLOONFS} && while read -r p; do
        if [ -f "${p}" ] && [ ! -L "${p}" ] && \
           head -c 4 "${p}" | cmp -s - ${WORK}/elf_magic; then
          case "$(od -An -tu2 -j16 -N2 "${p}" | tr -d ' ')" in
            2|3) echo "/${p}" ;;
          esac
        fi
      done < ${ADDED} | in_root /bin/bash -c 'while read -r e; do
        /lib64/ld-linux-x86-64.so.2 --library-path "${e%/*}" --list "$e" \
          2>&1 | grep -E "not found|error while loading"; done; true')
if [ -n "${BAD}" ]; then
  echo "${IMAGE} does not load in the root of the toy:"
  echo "${BAD}"
  echo "build ${IMAGE}.tar.gz and the targz_store from the same mirror"
  exit 1
fi
# @python: the standard library is cut to what the demo scripts import. Its
# sitecustomize tells a module cut by mistake (stderr and /dev/log of the
# container); the imports of the scripts are tried in the root of the toy.
if [ -s ${PYMODS} ]; then
  PYLIB=$(ls -d ${CLOONFS}/usr/lib/python3.[0-9]* 2>/dev/null)
  if [ $(echo ${PYLIB} | wc -w) != 1 ]; then
    echo "  TOY-PY KERR not one python in ${CLOONFS}/usr/lib: ${PYLIB}"
    exit 1
  fi
  rm -f ${PYLIB}/sitecustomize.py
  cat > ${PYLIB}/sitecustomize.py << "EOF"
# cloonix toy: this standard library is cut to what the demo scripts import
# (toy_targz/lib/special_needs.py, @python). A module cut by mistake is told
# on stderr and to /dev/log; "@python:<module>" in the needs of the toy adds it.
import sys

CLOONIX_TOY_HOOK = 1


def _cloonix_toy_hook(t, v, tb, _prev=sys.excepthook):
    if issubclass(t, ImportError):
        line = ("KERR: toy python %s: %s: %s, not in this toy, add "
                "@python:%s to the needs of the toy"
                % (sys.argv[0], t.__name__, v, getattr(v, "name", None) or "?"))
        sys.stderr.write(line + "\n")
        try:
            import socket
            s = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
            s.connect("/dev/log")
            s.send(("<11>cloonix-toy-python: " + line).encode())
            s.close()
        except Exception:
            pass
    _prev(t, v, tb)


sys.excepthook = _cloonix_toy_hook
EOF
  chmod 644 ${PYLIB}/sitecustomize.py
  PYSTART=$(in_root /usr/bin/python3 -c "pass" 2>&1)
  if [ -n "${PYSTART}" ]; then
    echo "  TOY-PY KERR the python of the toy says at its start: ${PYSTART}"
    exit 1
  fi
  PYCHECK='import importlib, sys
import sitecustomize
bad = 0 if getattr(sitecustomize, "CLOONIX_TOY_HOOK", 0) else 1
if bad:
    print("  TOY-PY KERR the sitecustomize of the toy is not the one loaded")
for m in sys.argv[1:]:
    try:
        importlib.import_module(m)
    except Exception as e:
        print("  TOY-PY KERR %s: %s: %s" % (m, type(e).__name__, e))
        bad = 1
sys.exit(bad)'
  if ! in_root /usr/bin/python3 -c "${PYCHECK}" $(cat ${PYMODS}); then
    echo "  TOY-PY KERR the demo scripts do not import in the root of the toy"
    echo "  add @python:<module> to ${SPEC_DEMO}/needs for each one above"
    exit 1
  fi
  echo "  TOY-PY ok: $(wc -l < ${PYMODS}) modules import in the root of the toy:" \
       $(cat ${PYMODS})
fi
SIZE2=$(du -sk ${ROOTFS} | cut -f1)

. ${HERE}/lib/pack_toy.sh
pack_toy ${WORK} ${TOY_NAME}
mv -f ${WORK}/${TOY_NAME}.tar.xz ${HOME}
rm -rf ${WORK}
echo "demos: ${KEEP} and ${AREA}/: $(cd ${SPEC_DEMO} && echo *.py), on self_rootfs"
echo "kvm: $((SIZE0 - SIZE1)) kB removed, libraries:${GONE}"
echo "${IMAGE}: ${NB_ADDED} files, $((SIZE2 - SIZE1)) kB merged into rootfs/cloonix/cloonfs"
echo END SCRIPT ${HOME}/${TOY_NAME}.tar.xz DONE
