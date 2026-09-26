#!/bin/bash
# Do the C and the python agree on the numbers that go on the wire?
set -e
HERE=$(cd $(dirname $0) && pwd)
WIRE=${HERE}/../cloonix/cloonsrc/glob_include/glob_wire.h
PYCLI=${HERE}/../cloonix/cloonix
TMP=$(mktemp -d)
trap "rm -rf ${TMP}" EXIT

# Two sorted lists, the lines of one only as - and + in $3; true when none.
# comm, not diff: diffutils is not on every machine (rocky).
differ()
{
  { comm -23 "$1" "$2" | sed 's/^/-/'; comm -13 "$1" "$2" | sed 's/^/+/'; } > "$3"
  [ ! -s "$3" ]
}

# What the C says: ask the compiler, not a regex.
{
  echo '#include <stdio.h>'
  echo '#include "glob_wire.h"'
  echo 'int main(void){'
  grep -oE '^(DOORS_TYPE|DOORS_VAL)_[A-Z0-9_]+ *=' ${PYCLI} | sed 's/ *=//' | sort -u |
  while read n; do
    c=$(echo ${n} | tr 'A-Z' 'a-z')
    echo "  printf(\"${n} %d\\n\", ${c});"
  done
  echo '  return 0; }'
} > ${TMP}/probe.c

if ! cc -I${HERE}/../cloonix/cloonsrc/glob_include -o ${TMP}/probe ${TMP}/probe.c 2>${TMP}/err; then
  echo "the python client names a constant the C no longer has:"
  grep -E 'undeclared|error' ${TMP}/err | head
  exit 1
fi
${TMP}/probe | sort > ${TMP}/from_c

grep -oE '^(DOORS_TYPE|DOORS_VAL)_[A-Z0-9_]+ *= *[0-9]+' ${PYCLI} |
  sed 's/ *= */ /' | sort > ${TMP}/from_py

if differ ${TMP}/from_py ${TMP}/from_c ${TMP}/diff; then
  echo "wire constants agree ($(wc -l < ${TMP}/from_c) checked)"
else
  echo "############################################################"
  echo "WIRE CONSTANTS DISAGREE"
  echo "  -  what cloonix/cloonix (python) sends"
  echo "  +  what glob_wire.h says"
  echo "############################################################"
  cat ${TMP}/diff
  exit 1
fi

CVER=$(grep -oE '^#define CLOONIX_WIRE_VERSION +[0-9]+' ${WIRE} | grep -oE '[0-9]+$')
PVER=$(grep -oE '^CLOONIX_WIRE_VERSION *= *[0-9]+' ${PYCLI} | grep -oE '[0-9]+$')
if [ "${CVER}" != "${PVER}" ]; then
  echo "############################################################"
  echo "WIRE VERSION DISAGREES: glob_wire.h says ${CVER}, cloonix/cloonix says ${PVER}"
  echo "############################################################"
  echo "Bump CLOONIX_WIRE_VERSION in the python client with the header."
  exit 1
fi
echo "wire version ${CVER} on both sides"

BASE=${HERE}/wire_baseline.txt
{
  echo '#include <stdio.h>'
  echo '#include "glob_wire.h"'
  echo 'int main(void){'
  grep -v '^#' ${BASE} | awk 'NF==2 {printf "  printf(\"%s %%d\\n\", %s);\n", $1, $1}'
  echo '  return 0; }'
} > ${TMP}/base.c

if ! cc -I${HERE}/../cloonix/cloonsrc/glob_include -o ${TMP}/base ${TMP}/base.c 2>${TMP}/berr; then
  echo "############################################################"
  echo "A WIRE VALUE HAS BEEN REMOVED"
  echo "############################################################"
  grep -E 'undeclared|error' ${TMP}/berr | head
  echo
  echo "Rule 1 in glob_wire.h: a value is never removed, it is renamed to"
  echo "<name>_reserved_N and left where it is. Every peer of an earlier"
  echo "release depends on the ones after it keeping their numbers."
  exit 1
fi
${TMP}/base | sort > ${TMP}/now
grep -v '^#' ${BASE} | awk 'NF==2' | sort > ${TMP}/then

if differ ${TMP}/then ${TMP}/now ${TMP}/wdiff; then
  echo "no wire value has moved since 76 ($(wc -l < ${TMP}/then) checked)"
else
  echo "############################################################"
  echo "A WIRE VALUE HAS MOVED SINCE 76"
  echo "  -  what debug/wire_baseline.txt froze at 76"
  echo "  +  what glob_wire.h says now"
  echo "############################################################"
  cat ${TMP}/wdiff
  echo
  echo "This breaks every cloonix and every client of an earlier release,"
  echo "and it breaks them silently. Put the value back where it was."
  echo "Adding a value at the END of an enum is fine: append its line to"
  echo "debug/wire_baseline.txt in the same pass. Never edit a line that is"
  echo "already there to make this pass."
  exit 1
fi

CFG=${HERE}/../cloonix/cloonix_config.py

BAD=$(grep -n 'CLOONIX_VERSION=\|CLOONIX_NET:' ${CFG} |
      grep -vE '^[0-9]+:(CLOONIX_VERSION=|CLOONIX_NET:)' || true)

if [ -z "${BAD}" ]; then
  echo "cloonix_config.py keeps its keywords to its text"
else
  echo "############################################################"
  echo "A CONFIG KEYWORD SITS OUTSIDE THE TEXT OF cloonix_config.py"
  echo "############################################################"
  echo "${BAD}"
  echo
  echo "The C, the awk and the grep that read that file take the first"
  echo "match, wherever it is. Write the word away from its separator --"
  echo "the regexes use \\s*=\\s* and \\s*:\\s* for exactly this -- or say"
  echo "it in words instead."
  exit 1
fi
