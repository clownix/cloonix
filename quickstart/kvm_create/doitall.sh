#!/bin/bash
set -e

sudo echo "sudoer rights given"
mkdir -p /var/lib/cloonix/bulk

LIST="openwrt \
      alpine \
      plucky.sh \
      noble \
      fedora42 \
      bookworm \
      desktop_bookworm_gnome \
      desktop_bookworm_lxde \
      desktop_fedora41_kde"

for i in ${LIST}; do
if [ -e /var/lib/cloonix/bulk/${i}.qcow2 ]; then
  echo /var/lib/cloonix/bulk/${i}.qcow2
  echo already exists
  exit
fi
done

LIST="openwrt \
      alpine \
      fedora41 \
      noble \
      bookworm \
      bookworm_fr"

for i in ${LIST}; do
  echo BEGIN ${i} 
  sudo ./${i}.sh
  echo END ${i}  
done

LIST="desktop_bookworm_gnome \
      desktop_bookworm_lxde \
      desktop_fedora41_kde"
for i in ${LIST}; do
  echo BEGIN ${i} 
  ./${i}.sh
  echo END ${i}  
done

