#!/bin/bash
PARAMS="ram=2000 cpu=2 eth=s"
DIST=bullseye

#######################################################################
for NET in nemo mito; do
  is_started=$(cloonix_cli $NET pid |grep cloonix_server)
  if [ "x$is_started" == "x" ]; then
    printf "\nServer Not started, launching:"
    printf "\ncloonix_net $NET:\n"
    cloonix_net $NET
  else
    cloonix_cli $NET rma
  fi
done

echo waiting 2 sec
sleep 2


#######################################################################
for NET in nemo mito; do
  cloonix_gui $NET
  cloonix_cli $NET add kvm vm ${PARAMS} ${DIST}.qcow2 &
done

set +e
for NET in nemo mito; do
  while ! cloonix_ssh $NET vm "echo" 2>/dev/null; do
    echo vm not ready, waiting 5 sec
    sleep 5
  done
done
set -e
#----------------------------------------------------------------------
for NET in nemo mito; do
  echo cloonix_cli $NET add lan vm 0 lan
  cloonix_cli $NET add lan vm 0 lan
  echo done
done
#######################################################################
cloonix_ssh nemo vm "ip addr add dev eth0 1.1.1.1/24"
cloonix_ssh nemo vm "ip link set dev eth0 up"
cloonix_ssh mito vm "ip addr add dev eth0 1.1.1.2/24"
cloonix_ssh mito vm "ip link set dev eth0 up"
#----------------------------------------------------------------------

sleep 2
echo cloonix_cli nemo add d2d name_d2d mito
cloonix_cli nemo add d2d name_d2d mito 

for NET in nemo mito; do
  echo cloonix_cli $NET add lan name_d2d 0 lan
  cloonix_cli $NET add lan name_d2d 0 lan
  echo done
done

#----------------------------------------------------------------------
sleep 1
urxvt -title nemo -e cloonix_ssh nemo vm "iperf3 -s" &
sleep 2
urxvt -title mito -e cloonix_ssh mito vm "iperf3 -c 1.1.1.1 -t 10000" &
#----------------------------------------------------------------------
#sleep 30
#kill $(jobs -p)
echo DONE
