#/bin/bash
set +e


LIST="centos8 \
      fedora35 \
      jammy \
      hirsute \
      impish \
      bookworm \
      bullseye \
      tumbleweed"


for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

