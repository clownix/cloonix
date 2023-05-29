#/bin/bash
set -x
HERE=`pwd`
NET=nemo
LIST="lunar    \
      jammy    \
      bookworm \
      bullseye \
      fedora38 \
      fedora37 \
      centos9  \
      centos8  \
      tumbleweed"
#----------------------------------------------------------------------
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" != "x" ]; then
  cloonix_cli $NET kil
  echo waiting 20 sec
  sleep 20
fi
#----------------------------------------------------------------------
cd ${HERE}/../..
./allclean
cd ${HERE}/../../..
rm -rf /tmp/${NET}
mkdir -p /tmp/${NET}
tar zcvf /tmp/${NET}/sources.tar.gz ./sources
#----------------------------------------------------------------------
set +e
printf "\nServer launch:"
printf "\ncloonix_net $NET:\n"
cloonix_net $NET
echo waiting 2 sec
sleep 2
cloonix_gui $NET
#----------------------------------------------------------------------
for i in $LIST; do
  cloonix_cli $NET add kvm $i ram=3000 cpu=4 eth=s full_${i}.qcow2 &
done
#----------------------------------------------------------------------
for i in $LIST; do
  while ! cloonix_ssh $NET ${i} "echo" 2>/dev/null; do
    echo ${i} not ready, waiting 5 sec
    sleep 5
  done
done
#----------------------------------------------------------------------
for i in $LIST; do
  cloonix_scp $NET /tmp/${NET}/sources.tar.gz ${i}:/root
  cloonix_ssh $NET ${i} "tar xvf sources.tar.gz"
  cloonix_ssh $NET ${i} "rm sources.tar.gz"
done
#----------------------------------------------------------------------
for i in $LIST; do
  cloonix_ssh $NET ${i} "cd sources ; ./doitall; sleep 5" &
done
echo ENDED
sleep 10000
