#!/bin/bash
HERE=`pwd`

cd ${HERE}/zipbasic
./step1_zipbasic_qcow2.sh
./step2_zipbasic_zip.sh

cd ${HERE}/zipfrr
./create_zipfrr.sh
