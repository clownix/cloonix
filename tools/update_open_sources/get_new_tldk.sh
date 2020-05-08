#!/bin/bash
HERE=`pwd`
TARGZ=${HERE}/../../targz_store
NAME=tldk

rm -rf ${NAME}
rm -f ${NAME}.tar.gz
rm -f ${TARGZ}/${NAME}.tar.gz

git clone --depth=1  https://gerrit.fd.io/r/tldk

for i in ".git" "dpdk" "app" "doc" "mk" "test" "vagrant"; do
  rm -rf ${HERE}/${NAME}/$i
done
for i in  ".gitreview .gitignore" \
          "INFO.yaml" "Makefile" "README" \
          "examples/Makefile" \
          "examples/l4fwd/Makefile" \
          "lib/Makefile" \
          "lib/libtle_dring/Makefile" \
          "lib/libtle_timer/Makefile" \
          "lib/libtle_l4p/Makefile" \
          "lib/libtle_misc/Makefile" \
          "lib/libtle_memtank/Makefile" ; do
  rm -f ${HERE}/${NAME}/$i
done
for i in "examples/l4fwd/udp.h" \
         "examples/l4fwd/pkt.c" \
         "examples/l4fwd/common.h" \
         "examples/l4fwd/parse.c" \
         "examples/l4fwd/netbe.h" \
         "examples/l4fwd/port.h" \
         "examples/l4fwd/parse.h" \
         "lib/libtle_l4p/tcp_misc.h" \
         "lib/libtle_l4p/tcp_rxtx.c" \
         "lib/libtle_l4p/misc.h" \
         "lib/libtle_l4p/stream.h" \
         "lib/libtle_l4p/udp_rxtx.c" ; do

  sed -i s'/ipv4_hdr/rte_ipv4_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/ipv6_hdr/rte_ipv6_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/udp_hdr/rte_udp_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/tcp_hdr/rte_tcp_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_MTU/RTE_ETHER_MTU/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_TYPE_IPv4/RTE_ETHER_TYPE_IPV4/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_TYPE_IPv6/RTE_ETHER_TYPE_IPV6/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_TYPE_VLAN/RTE_ETHER_TYPE_VLAN/g' ${HERE}/${NAME}/$i
  sed -i s'/ether_hdr/rte_ether_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/vlan_hdr/rte_vlan_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/IPV4_IHL_MULTIPLIER/RTE_IPV4_IHL_MULTIPLIER/g' ${HERE}/${NAME}/$i
  sed -i s'/ARP_OP_REPLY/RTE_ARP_OP_REPLY/g' ${HERE}/${NAME}/$i
  sed -i s'/ARP_OP_REQUEST/RTE_ARP_OP_REQUEST/g' ${HERE}/${NAME}/$i
  sed -i s'/ARP_HRD_ETHER/RTE_ARP_HRD_ETHER/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_CRC_LEN/RTE_ETHER_CRC_LEN/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_MAX_LEN/RTE_ETHER_MAX_LEN/g' ${HERE}/${NAME}/$i
  sed -i s'/ether_addr/rte_ether_addr/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_MAX_JUMBO_FRAME_LEN/RTE_ETHER_MAX_JUMBO_FRAME_LEN/g' ${HERE}/${NAME}/$i
  sed -i s'/ETHER_TYPE_ARP/RTE_ETHER_TYPE_ARP/g' ${HERE}/${NAME}/$i
  sed -i s'/IPV4_HDR_IHL_MASK/RTE_IPV4_HDR_IHL_MASK/g' ${HERE}/${NAME}/$i
  sed -i s'/IPV4_HDR_DF_FLAG/RTE_IPV4_HDR_DF_FLAG/g' ${HERE}/${NAME}/$i
  sed -i s'/arp_hdr/rte_arp_hdr/g' ${HERE}/${NAME}/$i
  sed -i s'/arp_ipv4/rte_arp_ipv4/g' ${HERE}/${NAME}/$i
  sed -i s'/arp_hrd/arp_hardware/g' ${HERE}/${NAME}/$i
  sed -i s'/arp_pro/arp_protocol/g' ${HERE}/${NAME}/$i
  sed -i s'/arp_op/arp_opcode/g' ${HERE}/${NAME}/$i
  sed -i s'/lcore_config\[lc\].f/NULL/g' ${HERE}/${NAME}/$i

done

sed -i '22 a #define __USE_GNU' ${HERE}/${NAME}/examples/l4fwd/parse.c
sed -i '23 a #include <sched.h>' ${HERE}/${NAME}/examples/l4fwd/parse.c


tar zcvf ${NAME}.tar.gz ${NAME}
rm -rf ${NAME}
mv ${NAME}.tar.gz ${TARGZ}


