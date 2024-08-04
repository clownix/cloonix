#!/bin/bash
HERE=`pwd`

cd ${HERE}/zipbazic
./create_zipbazic.sh

cd ${HERE}/zipcloonix
./create_zipcloonix.sh

cd ${HERE}/zipfrr
./create_zipfrr.sh
