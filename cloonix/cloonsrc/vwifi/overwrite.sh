#!/bin/bash

HERE=`pwd`

./allclean

./cmd

cd ${HERE}/../create_insider_agents_iso
./cmd_mk_iso
cd ${HERE}

sudo rm -rfv /usr/libexec/cloonix/insider_agents
sudo mv -v ${HERE}/../insider_agents /usr/libexec/cloonix/



for i in "cloonix-vwifi-add-interfaces" \
         "cloonix-vwifi-client" \
         "cloonix-vwifi-ctrl" \
         "cloonix-vwifi-server" \
         "cloonix-vwifi-spy" ; do
  sudo rm -fv /usr/libexec/cloonix/cloonfs/bin/${i}
  sudo mv -v ${i} /usr/libexec/cloonix/cloonfs/bin/
done

cd ../../../
./allclean
