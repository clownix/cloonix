#/bin/bash
set +e

LIST="centos8 \
      fedora31 \
      opensuse152 \
      eoan \
      buster \
      bullseye"

for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}
  echo END ${i}  
done

