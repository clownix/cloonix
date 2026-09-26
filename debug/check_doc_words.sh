#!/bin/bash
# Does every line of the docs carry a fact?
set -e
HERE=$(cd $(dirname $0) && pwd)
ROOT=${HERE}/..
DOCS="${ROOT}/httpdoc"

# Judgement: tells the reader what to think of a fact.
judgement="worth [a-z]+|no matter of taste|a matter of taste|
is the point|the point of|the whole point|not cosmetic|
worse than|better than|the best|the easiest|no better|
easy to|hard to|difficult|trivial|useless|pointless|
elegant|clever|neat|beautiful|ugly|nasty|horrible|
surprising|confusing|annoying|painful|sensible|silly|
powerful|seamless|robust|intuitive|user-friendly|revolutionary|
blazing|lightning|effortless|state of the art|best-in-class|
beware|unfortunately|sadly|obviously|of course|needless to say"

# Filler: removing it changes nothing.
filler="simply|merely|basically|essentially|actually|
very |quite |really |just simply|a lot of|
it should be noted|note that|please note|as you can see|
in order to|the fact that"

fail=0
report() {
  local kind="$1" pat="$2"
  local out
  out=$(grep -rniE "\b(${pat})\b" ${DOCS} \
        --include=*.rst --include=*.md --include=*.html \
        --exclude-dir=build_user --exclude-dir=build_dev 2>/dev/null \
        | grep -F -v -f ${HERE}/check_doc_words_allow.txt || true)
  if [ -n "${out}" ]; then
    echo "${kind}:"
    echo "${out}" | sed 's/^/    /'
    fail=1
  fi
}

# The lists are written over several lines for reading; join them back.
judgement=$(echo "${judgement}" | tr -d '\n')
filler=$(echo "${filler}" | tr -d '\n')

report "JUDGEMENT, not a fact" "${judgement}"
report "FILLER, carries nothing" "${filler}"

if [ ${fail} -eq 0 ]; then
  echo "every line of the docs carries a fact and no judgement"
else
  echo
  echo "each line above says how to feel about something, or says nothing."
  echo "state what the software does instead, or delete the words."
  exit 1
fi
