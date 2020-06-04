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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#define ALLOW_EXPERIMENTAL_API
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
#include "rxtx.h"
#include "dhcp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "rxq_dpdk.h"


static void *g_jfs;

/****************************************************************************/
static void rxtx_vhost_rx(struct rte_mbuf *mbuf)
{
  int len, data_len, l3_len, offset, tcp_hdr_len;
  uint8_t l4_proto, *data, *smac, *dmac;
  uint16_t ethertype;
  struct rte_ether_hdr *eth_hdr;
  struct rte_ipv4_hdr *ipv4_hdr;
  struct rte_udp_hdr *udp_hdr;
  struct rte_tcp_hdr *tcp_hdr;
  struct rte_icmp_hdr *icmp_hdr;
  void *l3_hdr;
  len = (int) (mbuf->pkt_len);
  eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
  ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);
  smac = (uint8_t *) (&eth_hdr->s_addr);
  dmac = (uint8_t *) (&eth_hdr->d_addr);
  if (ethertype == RTE_ETHER_TYPE_ARP)
    {
    dhcp_arp_management(mbuf);
    } 
  else if (ethertype == RTE_ETHER_TYPE_IPV4)
    {
    offset =  sizeof(struct rte_ether_hdr);
    offset += sizeof(struct rte_ipv4_hdr);
    l3_hdr = (uint8_t *)eth_hdr + sizeof(struct rte_ether_hdr);
    l3_len = sizeof(struct rte_ipv4_hdr);
    ipv4_hdr = (struct rte_ipv4_hdr *)l3_hdr;
    l4_proto = ipv4_hdr->next_proto_id;
    if (l4_proto == IPPROTO_ICMP)
      {
      icmp_hdr = (struct rte_icmp_hdr *) ((uint8_t *)ipv4_hdr +
                                         sizeof(struct rte_ipv4_hdr));
      if ((icmp_hdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST) &&
          (icmp_hdr->icmp_code == 0))
        {
        icmp_input(eth_hdr, ipv4_hdr, icmp_hdr, mbuf);
        }
      else
        {
        KERR("PROTO ICMP NOT MANAGED len: %d", len);
        rte_pktmbuf_free(mbuf);
        }
      }
    else if (l4_proto == IPPROTO_UDP)
      {
      offset += sizeof(struct rte_udp_hdr); 
      data_len = (int) (len - offset);
      if (data_len < 0)
        KOUT("%d %d", len, offset);
      data = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, offset);
      udp_hdr = (struct rte_udp_hdr *)(((uint8_t *)ipv4_hdr) + l3_len);
      if ((rte_be_to_cpu_16(udp_hdr->dst_port) == BOOTP_SERVER) &&
          (rte_be_to_cpu_16(udp_hdr->src_port) == BOOTP_CLIENT))
        {
        dhcp_input(smac, dmac, data_len, data);
        }
      else
        {
        udp_input(smac, dmac, ipv4_hdr, udp_hdr, data_len, data);
        }
      rte_pktmbuf_free(mbuf);
      }
    else if (l4_proto == IPPROTO_TCP)
      {
      tcp_hdr = (struct rte_tcp_hdr *)(((uint8_t *)ipv4_hdr) + l3_len);
      tcp_hdr_len = ((tcp_hdr->data_off & 0xf0) >> 2); 
      if (tcp_hdr_len < sizeof(struct rte_tcp_hdr))
        KOUT("%d %d", tcp_hdr_len, (int)sizeof(struct rte_tcp_hdr)); 
      offset += tcp_hdr_len; 
      if (len < offset)
        KOUT("%d %d", len, offset); 
      data_len = (int) (len - offset);
      data = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, offset);
      tcp_input(smac, dmac, ipv4_hdr, tcp_hdr, data_len, data);
      rte_pktmbuf_free(mbuf);
      }
    else
      {
      KERR("PROTO NOT MANAGED %X len: %d", l4_proto, len);
      rte_pktmbuf_free(mbuf);
      }
    }
  else if (ethertype == RTE_ETHER_TYPE_IPV6)
    {
    rte_pktmbuf_free(mbuf);
    }
  else
    {
    KERR("EHTERTYPE NOT MANAGED %X len: %d", ethertype, len);
    rte_pktmbuf_free(mbuf);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_job_for_select(int ref_id, void *data)
{ 
  struct rte_mbuf *mbuf;
  if (ref_id != 0xBBB)
    KOUT(" ");
  mbuf = rxq_dpdk_dequeue();
  while(mbuf)
    {
    rxtx_vhost_rx(mbuf);
    mbuf = rxq_dpdk_dequeue();
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rxtx_job_trigger(void)
{
  job_for_select_request(g_jfs, fct_job_for_select, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rxtx_init(void)
{
  int llid1, llid2;
  job_for_select_init();
  g_jfs = job_for_select_alloc(0xBBB, &llid1, &llid2);
}
/*--------------------------------------------------------------------------*/

