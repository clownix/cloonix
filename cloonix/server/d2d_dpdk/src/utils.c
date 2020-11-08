/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_arp.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"

#define EMPTY_HEAD 0


static uint32_t g_mac_len;
static uint32_t g_ip_len;
static uint32_t g_udp_len;
static uint32_t g_tcp_len;

//static uint16_t sum_calc(int len, uint8_t *buff, uint8_t *pseudo_header)
//{
//  int i, sum = 0;
//  for (i=0; i<len; i=i+2)
//    {
//    if (i+1 == len)
//      {
//      sum += ((buff[i] << 8) & 0xFF00);
//      break;
//      }
//    else
//      sum += ((buff[i] << 8) & 0xFF00) + (buff[i+1] & 0xFF);
//    }
//  for (i=0; i<12; i=i+2)
//    sum += ((pseudo_header[i] << 8) & 0xFF00) + (pseudo_header[i+1] & 0xFF);
//  sum = (sum & 0xFFFF) + (sum >> 16);
//  sum = (sum & 0xFFFF) + (sum >> 16);
//  sum = ~sum;
//  return ((uint16_t) sum);
//}
//static void utils_fill_tcp_csum(uint8_t  *buf_ip, uint32_t tcp_len)
//{
//  int i;
//  uint8_t pseudo_header[12];
//  uint16_t checksum;
//  uint8_t *buf = &(buf_ip[g_ip_len]);
//  buf[16]  = 0;
//  buf[17]  = 0;
//  for (i=0; i< 8; i++)
//    pseudo_header[i] = buf_ip[12+i];
//  pseudo_header[8]  = 0;
//  pseudo_header[9]  = IPPROTO_TCP;
//  pseudo_header[10] = (tcp_len >> 8) & 0xFF;
//  pseudo_header[11] = tcp_len & 0xFF;
//  checksum = sum_calc(tcp_len, &(buf[0]), pseudo_header);
//  buf[16]  =  (checksum >> 8) & 0xFF;
//  buf[17]  =  checksum & 0xFF;
//}
//static void utils_fill_udp_csum(uint8_t  *buf_ip, uint32_t udp_len)
//{
//  int i;
//  uint8_t pseudo_header[12];
//  uint16_t checksum;
//  uint8_t *buf = &(buf_ip[g_ip_len]);
//  buf[6]  = 0;
//  buf[7]  = 0;
//  for (i=0; i< 8; i++)
//    pseudo_header[i] = buf_ip[12+i];
//  pseudo_header[8]  = 0;
//  pseudo_header[9]  = IPPROTO_UDP;
//  pseudo_header[10] = (udp_len >> 8) & 0xFF;
//  pseudo_header[11] = udp_len & 0xFF;
//  checksum = sum_calc(udp_len, &(buf[0]), pseudo_header);
//  buf[6]  =  (checksum >> 8) & 0xFF;
//  buf[7]  =  checksum & 0xFF;
//}
void checksum_compute(struct rte_mbuf *mbuf)
{
//  int offset;
//  uint8_t l4_proto, *data;
//  uint16_t ethertype;
//  struct rte_ether_hdr *eth_hdr;
//  struct rte_ipv4_hdr *ipv4_hdr;
//  void *l3_hdr;
//  uint32_t hlen, data_len;
//  data_len = (uint32_t) (mbuf->pkt_len);
//  if (data_len <= EMPTY_HEAD)
//    KOUT("%d", data_len);
//  data_len -= EMPTY_HEAD;
//  eth_hdr = rte_pktmbuf_mtod_offset(mbuf, struct rte_ether_hdr *, EMPTY_HEAD);
//  ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);
//  data = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, EMPTY_HEAD);
//  if (ethertype == RTE_ETHER_TYPE_IPV4)
//    {
//    offset =  sizeof(struct rte_ether_hdr);
//    offset += sizeof(struct rte_ipv4_hdr);
//    l3_hdr = (uint8_t *)eth_hdr + sizeof(struct rte_ether_hdr);
//    ipv4_hdr = (struct rte_ipv4_hdr *)l3_hdr;
//    l4_proto = ipv4_hdr->next_proto_id;
//    if (l4_proto == IPPROTO_UDP)
//      {
//      hlen = g_mac_len + g_ip_len;
//      utils_fill_udp_csum(data + g_mac_len, data_len - hlen);
//      }
//    else if (l4_proto == IPPROTO_TCP)
//      {
//      hlen = g_mac_len + g_ip_len;
//      utils_fill_tcp_csum(data + g_mac_len, data_len - hlen);
//      }
//    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_init(void)
{
  g_mac_len = sizeof(struct rte_ether_hdr);
  g_ip_len  = sizeof(struct rte_ipv4_hdr);
  g_udp_len = sizeof(struct rte_udp_hdr);
  g_tcp_len = sizeof(struct rte_tcp_hdr);
}
/*---------------------------------------------------------------------------*/

