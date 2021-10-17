#/bin/bash
set +e


LIST="centos8 \
      fedora34 \
      hirsute \
      bullseye \
      tumbleweed"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

