#!/bin/bash
HERE=`pwd`

cloonix_cli nemo kil 1>/dev/null 2>&1

cd ${HERE}/zipbasic
./step1_zipbasic_qcow2.sh
./step2_zipbasic_zip.sh

cd ${HERE}/zipfrr
./create_zipfrr.sh
