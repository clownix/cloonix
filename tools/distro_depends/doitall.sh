#/bin/bash
set +e

LIST="centos8 \
      fedora32 \
      opensuse152 \
      focal \
      buster \
      bullseye"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

