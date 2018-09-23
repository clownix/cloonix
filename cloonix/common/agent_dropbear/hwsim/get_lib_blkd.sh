#!/bin/bash
HERE=`pwd`
rm -rf ${HERE}/lib_blkd
cp -r ${HERE}/../../lib_blkd .
cp ${HERE}/../../glob_include/ioc_blkd.h ${HERE}/lib_blkd/src
