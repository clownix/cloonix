#!/bin/bash
./create_frr_zip.sh
./create_frr_cloonix.sh
cd ../../
./podman_transform.sh podman_frr_cloonix.zip 

exit


ID_IMAGE=$(podman images |grep podman_frr_cloonix | awk "{print \$3}")
podman run -it --privileged \
           -e DISPLAY=:0 -v /tmp/.X11-unix:/tmp/.X11-unix \
           -v /lib/modules:/lib/modules $ID_IMAGE /bin/bash


./spider_demo.sh

double-click on sod43 for example and do traceroute sod36 for example, I get:

sod43# traceroute sod36
traceroute to sod36 (36.0.0.1), 30 hops max, 46 byte packets
 1  43.0.0.29 (43.0.0.29)  0.775 ms  0.003 ms  0.003 ms
 2  22.0.0.10 (22.0.0.10)  0.002 ms  0.003 ms  0.002 ms
 3  14.0.0.4 (14.0.0.4)  0.002 ms  0.002 ms  0.002 ms
 4  13.0.0.3 (13.0.0.3)  0.090 ms  0.003 ms  0.003 ms
 5  12.0.0.8 (12.0.0.8)  0.002 ms  0.002 ms  0.002 ms
 6  24.0.0.22 (24.0.0.22)  0.070 ms  0.003 ms  0.003 ms
 7  sod36 (36.0.0.1)  0.153 ms  0.003 ms  0.002 ms
sod43# 


