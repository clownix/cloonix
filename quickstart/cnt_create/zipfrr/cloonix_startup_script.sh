#!/bin/bash
#----------------------------------------------------------------------------#

/usr/sbin/rsyslogd

case ${NODE_ID} in

  "tod1")
    NB=1
    IP0=10
    IP1=11
    ;;

  "tod2")
    NB=2
    IP0=11
    IP1=12
    ;;

  "tod3")
    NB=3
    IP0=12
    IP1=13
    ;;

  "tod4")
    NB=4
    IP0=13
    IP1=14
    ;;

  "tod5")
    NB=5
    IP0=14
    IP1=10
    ;;

  "cod1")
    NB=6
    IP0=10
    IP1=15
    IP2=16
    IP3=17
    ;;

  "cod2")
    NB=7
    IP0=11
    IP1=18
    IP2=19
    IP3=23
    ;;

  "cod3")
    NB=8
    IP0=12
    IP1=24
    IP2=20
    IP3=25
    ;;

  "cod4")
    NB=9
    IP0=13
    IP1=26
    IP2=21
    IP3=27
    ;;

  "cod5")
    NB=10
    IP0=14
    IP1=28
    IP2=22
    IP3=29
    ;;

  "nod1")
    NB=11
    IP0=17
    IP1=18
    ;;

  "nod2")
    NB=12
    IP0=23
    IP1=24
    ;;

  "nod3")
    NB=13
    IP0=25
    IP1=26
    ;;

  "nod4")
    NB=14
    IP0=27
    IP1=28
    ;;

  "nod5")
    NB=15
    IP0=15
    IP1=29
    ;;

  "nod11")
    NB=16
    IP0=15
    IP1=30
    ;;

  "nod12")
    NB=17
    IP0=16
    IP1=31
    ;;

  "nod13")
    NB=18
    IP0=17
    IP1=32
    ;;

  "nod21")
    NB=19
    IP0=18
    IP1=33
    ;;

  "nod22")
    NB=20
    IP0=19
    IP1=34
    ;;

  "nod23")
    NB=21
    IP0=23
    IP1=35
    ;;

  "nod31")
    NB=22
    IP0=24
    IP1=36
    ;;

  "nod32")
    NB=23
    IP0=20
    IP1=37
    ;;

  "nod33")
    NB=24
    IP0=25
    IP1=38
    ;;

  "nod41")
    NB=25
    IP0=26
    IP1=39
    ;;

  "nod42")
    NB=26
    IP0=21
    IP1=40
    ;;

  "nod43")
    NB=27
    IP0=27
    IP1=41
    ;;

  "nod51")
    NB=28
    IP0=28
    IP1=42
    ;;

  "nod52")
    NB=29
    IP0=22
    IP1=43
    ;;

  "nod53")
    NB=30
    IP0=29
    IP1=44
    ;;
  "lod11")
    NB=31
    IP0=30
    IP1=31
    ;;

  "lod12")
    NB=32
    IP0=31
    IP1=32
    ;;

  "lod13")
    NB=33
    IP0=32
    IP1=33
    ;;

  "lod21")
    NB=34
    IP0=33
    IP1=34
    ;;

  "lod22")
    NB=35
    IP0=34
    IP1=35
    ;;

  "lod23")
    NB=36
    IP0=35
    IP1=36
    ;;
  "lod31")
    NB=37
    IP0=36
    IP1=37
    ;;

  "lod32")
    NB=38
    IP0=37
    IP1=38
    ;;

  "lod33")
    NB=39
    IP0=38
    IP1=39
    ;;

  "lod41")
    NB=40
    IP0=39
    IP1=40
    ;;

  "lod42")
    NB=41
    IP0=40
    IP1=41
    ;;

  "lod43")
    NB=42
    IP0=41
    IP1=42
    ;;

  "lod51")
    NB=43
    IP0=42
    IP1=43
    ;;

  "lod52")
    NB=44
    IP0=43
    IP1=44
    ;;

  "lod53")
    NB=45
    IP0=44
    IP1=30
    ;;

  "sod30"|"sod31"|"sod32"|"sod33"|"sod34"|"sod35"|"sod36"|"sod37"|"sod38"|"sod39")
    NB=1
    IP0=${NODE_ID#sod}
    ;;
  "sod40"|"sod41"|"sod42"|"sod43"|"sod44")
    NB=1
    IP0=${NODE_ID#sod}
    ;;

   *)
    /bin/logger "STARTUP FAILURE NODE_ID=$NODE_ID"
    exit 1
esac

sed -i s"/__NODE_ID__/${NB}/" /etc/frr/frr.conf

if [ ! -z "$IP0" ]; then
  ifconfig eth0 ${IP0}.0.0.${NB}/24 up
  echo "network ${IP0}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP1" ]; then
  ifconfig eth1 ${IP1}.0.0.${NB}/24 up
  echo "network ${IP1}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP2" ]; then
  ifconfig eth2 ${IP2}.0.0.${NB}/24 up
  echo "network ${IP2}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

if [ ! -z "$IP3" ]; then
  ifconfig eth3 ${IP3}.0.0.${NB}/24 up
  echo "network ${IP3}.0.0.0/24 area 0.0.0.0" >> /etc/frr/frr.conf
fi

script -c "/usr/lib/frr/frrinit.sh start"
##############################################################################


