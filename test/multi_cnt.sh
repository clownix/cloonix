#!/bin/bash
NET=nemo
NUM=8

#######################################################################
is_started=$(cloonix_cli $NET pid |grep cloonix_server)
if [ "x$is_started" == "x" ]; then
  printf "\nServer Not started, launching:"
  printf "\ncloonix_net $NET:\n"
  cloonix_net $NET
else
  cloonix_cli $NET rma
fi

#######################################################################
cloonix_gui $NET
#----------------------------------------------------------------------



while [ 1 ]; do

for ((i=0;i<${NUM};i++)); do
  echo cloonix_cli nemo add cnt cnt${i} eth=ssss bullseye.img
  cloonix_cli nemo add cnt cnt${i} eth=ssss bullseye.img
done

for ((i=0;i<${NUM};i++)); do
  echo cloonix_cli nemo del sat cnt${i}
  cloonix_cli nemo del sat cnt${i}
done

done


