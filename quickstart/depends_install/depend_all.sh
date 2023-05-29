#/bin/bash
set -e

LIST="lunar    \
      jammy    \
      bookworm \
      bullseye \
      fedora38 \
      fedora37 \
      centos9  \
      centos8  \
      tumbleweed"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
  sleep 5
done

