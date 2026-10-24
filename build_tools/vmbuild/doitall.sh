#!/bin/bash
HERE=`pwd`
NET="mito"
QCOW2="builder.qcow2"
NAME="dev"
PARAMS="ram=8000 cpu=4 eth=1"
BULK="/var/lib/cloonix/bulk"
CLOONIX_TREE_ROOT=${HERE}/../..
# Every create_<toy>_toy_targz.sh of toy_targz, its image named in it.
TOYS=$(cd ${CLOONIX_TREE_ROOT}/toy_targz && ls create_*_toy_targz.sh | \
       sed 's/^create_\(.*\)_toy_targz\.sh$/\1/')
set +e
is_started=$(cloonix cli $NET dump pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix start $NET:\n"
  cloonix start $NET
else
  cloonix cli $NET del all
fi
set -e
if [ ! -e ${CLOONIX_TREE_ROOT}/cloonix/cloonix_config.py ]; then
  echo NOT FOUND:
  echo ${CLOONIX_TREE_ROOT}/cloonix/cloonix_config.py
  exit 1
fi
builder_up()
{
  cloonix cli ${NET} add kvm ${NAME} ${PARAMS} ${QCOW2}
  sleep 3
  set +e
  while ! cloonix cli ${NET} ssh ${NAME} "echo" 2>/dev/null; do
    echo ${NAME} not ready, waiting 5 sec
    sleep 5
  done
  set -e
}
# $1: the bulk of the builder, a name a line. The toy images not in it.
images_missing()
{
  local toy image
  for toy in ${TOYS}; do
    image=$(sed -n 's/^IMAGE="\(.*\)"$/\1/p' \
            ${CLOONIX_TREE_ROOT}/toy_targz/create_${toy}_toy_targz.sh)
    if ! echo "$1" | grep -qxF "${image}.tar.gz"; then
      echo -n " ${image}"
    fi
  done
}
# Three levels: seedbuilder.sh and builder.sh take from the repo, this one
# from nothing: the build finds all it needs in builder.qcow2, the toy images
# in its bulk.
if [ ! -e ${BULK}/${QCOW2} ]; then
  echo "ERROR ${BULK}/${QCOW2} does not exist: ./builder.sh makes it"
  exit 1
fi
builder_up
HAVE=$(cloonix cli ${NET} ssh ${NAME} "ls ${BULK} 2>/dev/null; true")
MISSING=$(images_missing "${HAVE}")
if [ -n "${MISSING}" ]; then
  echo "ERROR ${QCOW2} lacks the toy image(s):${MISSING}"
  echo "./builder.sh makes it again with every toy image, from the repo"
  exit 1
fi
rm -rf /tmp/cloonix_build_tree
mkdir -p /tmp/cloonix_build_tree
cd ${CLOONIX_TREE_ROOT}
./allclean
tar zcvf /tmp/cloonix_build_tree/sources.tar.gz .
cloonix cli ${NET} ssh ${NAME} "mkdir -p /root/sources"
cloonix cli ${NET} scp /tmp/cloonix_build_tree/sources.tar.gz ${NAME}:/root/sources
rm -rf /tmp/cloonix_build_tree
cloonix cli ${NET} ssh ${NAME} "cd /root/sources ; tar xvf sources.tar.gz ; rm sources.tar.gz"
cloonix cli ${NET} ssh ${NAME} "cd /root/sources; ./doitall"
cloonix cli ${NET} ssh ${NAME} "cd /root/cloonix-bundle; ./install_cloonfs"
cloonix cli ${NET} ssh ${NAME} "cd /root/sources/toy_targz; ./create_toy_targz.sh"
cloonix cli ${NET} scp ${NAME}:/root/cloonix-toy.tar.xz ${HOME}
cloonix cli ${NET} scp ${NAME}:/root/cloonix-bundle.tar.gz ${HOME}
# The specialised toys, their images in builder.qcow2 (builder.sh).
for TOY in ${TOYS}; do
  cloonix cli ${NET} ssh ${NAME} "cd /root/sources/toy_targz; ./create_${TOY}_toy_targz.sh"
  cloonix cli ${NET} scp ${NAME}:/root/cloonix-${TOY}-toy.tar.xz ${HOME}
done
