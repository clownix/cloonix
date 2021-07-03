#/bin/bash
set +e

LIST="centos8 \
      fedora34 \
      fedora33 \
      focal \
      groovy \
      hirsute \
      buster \
      bullseye \
      tumbleweed"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

