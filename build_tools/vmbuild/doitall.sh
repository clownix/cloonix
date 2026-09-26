#!/bin/bash
HERE=`pwd`
NET="mito"
DISTRO="trixie"
QCOW2="builder.qcow2"
NAME="dev"
CLOONIX_TREE_ROOT=${HERE}/../..
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
VERSION_NUM=$(grep -m1 '^CLOONIX_VERSION=' \
              ${CLOONIX_TREE_ROOT}/cloonix/cloonix_config.py)
VERSION_NUM=${VERSION_NUM#*=}
VERSION_NUM=${VERSION_NUM%-*}
rm -rf /tmp/cloonix_build_tree
mkdir -p /tmp/cloonix_build_tree
cd ${CLOONIX_TREE_ROOT}
./allclean
tar zcvf /tmp/cloonix_build_tree/sources.tar.gz .
cd ${CLOONIX_TREE_ROOT}/build_tools
PARAMS="ram=8000 cpu=4 eth=1"
cloonix cli ${NET} add kvm ${NAME} ${PARAMS} ${QCOW2}

sleep 3
set +e
while ! cloonix cli ${NET} ssh ${NAME} "echo" 2>/dev/null; do
  echo ${NAME} not ready, waiting 5 sec
  sleep 5
done
set -e
cloonix cli ${NET} ssh ${NAME} "mkdir -p /root/sources"
cloonix cli ${NET} scp /tmp/cloonix_build_tree/sources.tar.gz ${NAME}:/root/sources
rm -rf /tmp/cloonix_build_tree
cloonix cli ${NET} ssh ${NAME} "mkdir -p /var/lib/cloonix/bulk"
cloonix cli ${NET} ssh ${NAME} "cd /root/sources ; tar xvf sources.tar.gz ; rm sources.tar.gz"
cloonix cli ${NET} ssh ${NAME} "cd /root/sources; ./doitall"
cloonix cli ${NET} ssh ${NAME} "cd /root/cloonix-bundle-${VERSION_NUM}; ./install_cloonfs"
cloonix cli ${NET} ssh ${NAME} "cd /root/sources/toy_targz; ./create_toy_targz.sh"
cloonix cli ${NET} scp ${NAME}:/root/cloonix-toy-${VERSION_NUM}.tar.gz ${HOME}
cloonix cli ${NET} scp ${NAME}:/root/cloonix-bundle-${VERSION_NUM}.tar.gz ${HOME}

