#!/bin/bash
# What the linker throws away.
set -e
HERE=$(cd $(dirname $0) && pwd)
CLOONSRC=$(cd ${HERE}/../cloonix/cloonsrc && pwd)
OUT=/tmp/cloonix_deadcode
rm -rf ${OUT}
mkdir -p ${OUT}

GCC="gcc -ffunction-sections -fdata-sections -O0 -fno-inline"
GCLD="-Wl,--gc-sections -Wl,--print-gc-sections"
INCS="-I${CLOONSRC}/glob_include -I${CLOONSRC}/lib_io_clownix/include \
      -I${CLOONSRC}/lib_rpc_clownix/include"

# what the linker said, as "<function> <object>" lines
collect()
{
  grep 'removing unused section' \
  | grep -oE "'\.text\.[^']+' in file '[^']+'" \
  | sed "s/'\.text\.//;s/' in file '/ /;s/'//" | sort -u
}

# the two libraries, rebuilt with sections
for i in lib_io_clownix lib_rpc_clownix; do
  ( cd ${CLOONSRC}/${i}
    make clean >/dev/null 2>&1 || true
    make CC="${GCC}" >/dev/null 2>&1 )
done

# the daemons a Makefile builds
for i in uml_cloonix_switch suid_power doorways proxymous_passt cxp_cli \
         insider_agents/cloonix_cxp_srv; do
  n=$(basename ${i})
  ( cd ${CLOONSRC}/${i}
    make clean >/dev/null 2>&1 || true
    make CC="${GCC}" LDFLAGS="${GCLD}" 2>&1 | collect > ${OUT}/${n}.txt
    make clean >/dev/null 2>&1 || true )
done

# the agent: dead only if dead in its kvm and in its cnt build
( cd ${CLOONSRC}/insider_agents/cloonix_agent
  for side in kvm cnt; do
    [ ${side} = kvm ] && SIDE="main_kvm.c wifi.c" || SIDE="main_cnt.c wifi_none.c"
    make clean >/dev/null 2>&1 || true
    make CC="${GCC}" LDFLAGS="${GCLD}" SIDE="${SIDE}" 2>&1 | collect \
      > ${OUT}/agent_${side}.lst
  done
  make clean >/dev/null 2>&1 || true
  comm -12 ${OUT}/agent_kvm.lst ${OUT}/agent_cnt.lst > ${OUT}/cloonix_agent.txt )

# the gui server: one translation unit, its Makefile links with -rdynamic
( cd ${CLOONSRC}/jscloonix/jscloonix
  ${GCC} -g -D_GNU_SOURCE -w ${INCS} cloonix_gui_server.c -o ${OUT}/gui \
    ${GCLD} -Wl,--start-group -L${CLOONSRC}/lib_rpc_clownix -lrpc_clownix \
    -L${CLOONSRC}/lib_io_clownix -lio_clownix -Wl,--end-group \
    -lcrypto -lrt -lutil -lm -lpthread -Wl,--allow-multiple-definition 2>&1 \
    | collect > ${OUT}/jscloonix.txt )

# the satellites: the sources their cmd hands to meson
for i in ovs_drv ovs_snf ovs_a2b ovs_c2c ovs_nat_passt; do
  srcs=$(sed -n "/^sources = files(/,/)/p" ${CLOONSRC}/${i}/cmd \
         | grep -oE "'[^']+\.c'" | tr -d "'")
  ( cd ${CLOONSRC}/${i}
    ${GCC} -g -w -DALLOW_EXPERIMENTAL_API ${INCS} ${srcs} -o ${OUT}/${i} \
      ${GCLD} -L${CLOONSRC}/lib_io_clownix -L${CLOONSRC}/lib_rpc_clownix \
      -lio_clownix -lrpc_clownix -lcrypto -lm -lrt -lpthread 2>&1 \
      | collect > ${OUT}/${i}.txt )
done

for i in lib_io_clownix lib_rpc_clownix; do
  ( cd ${CLOONSRC}/${i}; make clean >/dev/null 2>&1 || true )
done

# cxp_wire.c is in the cxp client and the cxp server: dead only in both
W="obj/cxp_wire.o"
grep " ${W}$" ${OUT}/cloonix_cxp_srv.txt > ${OUT}/cxp_wire_srv.lst || true
grep -v " ${W}$" ${OUT}/cxp_cli.txt > ${OUT}/cxp_cli.lst || true
grep " ${W}$" ${OUT}/cxp_cli.txt | comm -12 - ${OUT}/cxp_wire_srv.lst \
  >> ${OUT}/cxp_cli.lst || true
sort -o ${OUT}/cxp_cli.txt ${OUT}/cxp_cli.lst

echo
echo "unreachable in the binary's own sources (recv_* stubs left out,"
echo "a weak default of the library stands in for each):"
echo
for f in ${OUT}/*.txt; do
  n=$(basename ${f} .txt)
  # cloonix_log: a weak copy in every object from glob_common.h, one kept
  own=$(grep -v '\.a(' ${f} | awk '{print $1}' | grep -v '^recv_' \
        | grep -v '^pthexec_' | grep -v '^cloonix_log$' | sort -u \
        | tr '\n' ' ')
  [ -n "${own}" ] && echo "  ${n}: ${own}"
done

# a binary that does not link the library drops nothing of it
NB=$(grep -l 'libio_clownix' ${OUT}/*.txt | wc -l)
echo
echo "lib_io_clownix, unused by all ${NB} binaries that link it:"
echo
cat ${OUT}/*.txt | grep 'libio_clownix' | grep -v '^cloonix_log ' \
  | sed -E 's/ .*\.a\(/ /;s/\.o\)//' | sort | uniq -c \
  | awk -v n=${NB} '$1==n{print "  "$3": "$2}' | sort

echo
echo "lib_rpc_clownix, unused by all five daemons that decode the rpc:"
echo
cat ${OUT}/uml_cloonix_switch.txt ${OUT}/suid_power.txt ${OUT}/doorways.txt \
    ${OUT}/proxymous_passt.txt ${OUT}/jscloonix.txt | grep 'librpc_clownix' \
  | sed -E 's/ .*\.a\(/ /;s/\.o\)//' | sort | uniq -c \
  | awk '$1==5{print "  "$3": "$2}' | sort

echo
echo "raw lists in ${OUT}/"
