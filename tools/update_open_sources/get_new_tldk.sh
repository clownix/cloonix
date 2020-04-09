#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
rm -rf tldk
rm -f tldk.tar.gz
rm -f ${TARGZ}/tldk.tar.gz
# 
git clone --depth=1  https://gerrit.fd.io/r/tldk
#https://github.com/FDio/tldk.git

tar zcvf tldk.tar.gz tldk
rm -rf tldk
mv tldk.tar.gz ${TARGZ}


