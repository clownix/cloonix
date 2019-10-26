#!/bin/bash
HERE=`pwd`
COMMON=${HERE}/../../common
rm -rf ${HERE}/lib_blkd
cp -r ${COMMON}/lib_blkd .
cp ${COMMON}/glob_include/ioc_blkd.h ${HERE}/lib_blkd/src
