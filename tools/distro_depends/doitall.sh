#/bin/bash
set +e

LIST="centos8 \
      fedora32 \
      fedora33 \
      opensuse153 \
      focal \
      groovy \
      hirsute \
      buster \
      bullseye"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

