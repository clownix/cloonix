#/bin/bash
HERE=`pwd`
NET=nemo
LIST="centos8 \
      buster \
      bullseye \
      fedora31 \
      opensuse152 \
      eoan" 


#######################################################################
is_started=$(cloonix_cli $NET lst |grep cloonix_net)
if [ "x$is_started" != "x" ]; then
  cloonix_cli $NET kil
  echo waiting 20 sec
  sleep 20
fi

cd ${HERE}/../..
./allclean
cd ${HERE}/../../..
mkdir -p /tmp/${NET}
tar zcvf /tmp/${NET}/sources.tar.gz ./sources

set +e
printf "\nServer launch:"
printf "\ncloonix_net $NET:\n"
cloonix_net $NET
echo waiting 2 sec
sleep 2
cloonix_gui $NET
#----------------------------------------------------------------------


#######################################################################
for DISTRO in $LIST; do
  cloonix_cli $NET add kvm $DISTRO ram=8000 cpu=8 eth=v full_${DISTRO}.qcow2 &
done

for DISTRO in $LIST; do
  while ! cloonix_ssh $NET ${DISTRO} "echo" 2>/dev/null; do
    echo ${DISTRO} not ready, waiting 5 sec
    sleep 5
  done
done

for DISTRO in $LIST; do
  cloonix_scp $NET /tmp/${NET}/sources.tar.gz ${DISTRO}:/root
  cloonix_ssh $NET ${DISTRO} "tar xvf sources.tar.gz"
  cloonix_ssh $NET ${DISTRO} "rm sources.tar.gz"
done

for DISTRO in $LIST; do
  cloonix_ssh $NET ${DISTRO} "cd sources ; ./doitall" &
done
sleep 10000
