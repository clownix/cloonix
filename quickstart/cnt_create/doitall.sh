#!/bin/bash
HERE=`pwd`
BULK="/var/lib/cloonix/bulk"
rm -rf ${BULK}/alpine
rm -rf ${BULK}/trixie
rm -f ${BULK}/alpine.zip
rm -f ${BULK}/trixie.zip

./alpine.sh
./trixie.sh

cd ${BULK}/alpine
zip -yrq ../alpine.zip .

cd ${BULK}/trixie
zip -yrq ../trixie.zip .
