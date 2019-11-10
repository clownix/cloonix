#!/bin/bash
HERE=`pwd`
rm -f qmp_diag
cd qmp
make clean
make
mv qmp_diag $HERE
make clean
