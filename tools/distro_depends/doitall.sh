#/bin/bash
set +e


LIST="centos8 \
      fedora35 \
      hirsute \
      impish \
      bookworm \
      bullseye \
      tumbleweed"

LIST="tumbleweed"



for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

