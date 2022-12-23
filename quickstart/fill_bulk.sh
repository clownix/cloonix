#!/bin/bash

HERE=`pwd`
set -e
cd ${HERE}/create_kvm 
./doitall.sh

cd ${HERE}/create_cnt
./doitall.sh
