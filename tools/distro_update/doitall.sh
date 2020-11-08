#/bin/bash
set +e

LIST="centos8 \
      fedora32 \
      fedora33 \
      opensuse152 \
      focal \
      groovy \
      buster \
      bullseye"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

