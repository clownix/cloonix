#!/bin/bash
# Do the docs still describe binaries that exist?
set -e
HERE=$(cd $(dirname $0) && pwd)
ROOT=${HERE}/..
PACK=${ROOT}/build_tools/pack_cloonfs
DOCS="${ROOT}/httpdoc/xdoc_dev ${ROOT}/httpdoc/xdoc_user"

# what ends up in the delivery, from the packing list itself
known=$(grep -oE 'cloonix-[a-z0-9-]+' ${PACK} | sort -u)
# plus the ones built or delivered under names pack_cloonfs does not spell out.
known="${known}
$(cat ${HERE}/check_docs_known.txt 2>/dev/null)"

bad=0
for d in ${DOCS}; do
  [ -d "${d}" ] || continue
  # A trailing dash, number or __LAST__ is a versioned tarball name, not a binary.
  for n in $(grep -rhoE 'cloonix-[a-z0-9_-]+(-__LAST__)?' ${d} 2>/dev/null \
             | grep -v -- '-$' | grep -vE -- '-([0-9]+|__LAST__)$' \
             | sort -u); do
    if ! echo "${known}" | grep -qx "${n}"; then
      echo "STALE DOC: ${n} is named in ${d} but is not in the delivery"
      grep -rln "${n}" ${d} | sed 's/^/    /'
      bad=1
    fi
  done
done

if [ ${bad} -eq 0 ]; then
  echo "docs name no binary that does not exist"
else
  echo
  echo "either the doc is out of date, or pack_cloonfs forgot something"
fi

# Does a book write a version by hand instead of __LAST__?
SHAPES='cloonix-(toy-|bundle-)?[0-9]+|build-[0-9]+|v[0-9]+-[0-9]+|cloonix [0-9]{2}'

for d in ${DOCS}; do
  [ -d "${d}" ] || continue
  hits=$(grep -rnE "${SHAPES}" ${d} --include='*.rst' \
         2>/dev/null \
         | grep -v '/release.rst:' || true)
  if [ -n "${hits}" ]; then
    echo "VERSION BY HAND: a book substitutes __LAST__, so this will rot"
    echo "${hits}" | sed 's/^/    /'
    bad=1
  fi
done

if [ ${bad} -eq 0 ]; then
  echo "no version written by hand in the books"
fi

# Does a documented capture filter use a word pcap_filter.c does not have?
hits=$(grep -rnE 'sniff +[^ ]+ +([^ -][^ ]* +)?--dump' ${DOCS} --include='*.rst' \
       --include='*.html' 2>/dev/null | python3 -c '
import re, sys
ok = set("ip ip6 arp tcp udp icmp icmp6 src dst host net port portrange "
         "less greater and or not".split())
for line in sys.stdin:
    flt = re.split(r" {3,}|<", line.split("--dump", 1)[1].rstrip())[0]
    for w in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", flt):
        if w.lower() not in ok:
            print("    " + line.rstrip()); break
' || true)
if [ -n "${hits}" ]; then
  echo "FILTER WORD: not in the grammar of the capture filter"
  echo "${hits}"
  bad=1
else
  echo "every documented capture filter uses the words of the server"
fi

exit ${bad}
