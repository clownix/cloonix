#!/bin/bash

HERE=`pwd`
set -e
cd ${HERE}/kvm 
./doitall.sh

cd ${HERE}/cnt
./doitall.sh
