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
/* Original copy: https://github.com/quiqfield/flowatcher                    */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <libwebsockets.h>

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
#include <rte_hash.h>
#include <rte_jhash.h>


#include "io_clownix.h"
#include "flow_tab.h"

#define IPPROTO_OSPFIGP 89

struct flow_table *g_flow_table_v4[2];
struct lws_context *g_wss_context;
struct lws_protocols g_wss_protocol[3];

/*****************************************************************************/
static uint32_t roundup32(uint32_t x)
{
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  x = x > RTE_HASH_ENTRIES_MAX ? RTE_HASH_ENTRIES_MAX : x;
  return x;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static struct flow_table *create_flow_table(char *name, uint32_t flow_table_size)
{
  struct flow_table *table;
  uint32_t size, keylen, len;
  struct rte_hash_parameters params;
  memset(&params, 0, sizeof(struct rte_hash_parameters));
  size = roundup32(flow_table_size);
  keylen = (uint32_t) sizeof(struct fivetuple_v4);
  params.name = name;
  params.entries = size;
  params.key_len = keylen;
  params.hash_func = rte_jhash;
  params.hash_func_init_val = 0;
  params.socket_id = (int) rte_socket_id();
  len = sizeof(struct flow_table) + sizeof(struct flow_entry) * size;
  table = (struct flow_table *) rte_zmalloc("flow_table", len,  0);
  if (table == NULL)
    KOUT("rte_zmalloc ERROR");
  table->handler = rte_hash_create(&params);
  if (table->handler == NULL)
    KOUT("rte_hash_create ERROR");
  return table;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
struct flow_table *get_flow_table_v4(int dir)
{
  return (g_flow_table_v4[dir]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
struct lws_context *get_wss_context(void)
{
  return g_wss_context;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
struct lws_protocols *get_wss_protocol(int val)
{
  return (&(g_wss_protocol[val]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_flow_entry(int dir, uint64_t timestamp, uint32_t pktlen,
                              struct fivetuple_v4 *fivetuple)
{
  struct flow_entry *entry;
  struct flow_table *table = g_flow_table_v4[dir];
  int32_t key = rte_hash_add_key(table->handler, fivetuple);
  if (key >= 0)
    {
    entry = &table->entries[key];
    if (entry->start_tsc == 0)
      {
      table->flow_cnt++;
      entry->start_tsc = timestamp;
      entry->last_tsc = timestamp;
      entry->pkt_cnt = 1;
      entry->byte_cnt = pktlen;
      rte_memcpy(&entry->key, fivetuple, sizeof(struct fivetuple_v4));
      }
    else
      {
      entry->last_tsc = timestamp;
      entry->pkt_cnt++;
      entry->byte_cnt += pktlen;
      }
    }
  else
    KERR("ERROR rte_hash_add_key %d", key);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void app_tx_flow(int dir, uint16_t nb_bufs, struct rte_mbuf **bufs)
{
  struct rte_mbuf *buf;
  struct rte_ether_hdr *eth_hdr;
  struct rte_ipv4_hdr *rte_ipv4_hdr;
  struct rte_tcp_hdr *rte_tcp_hdr;
  struct rte_udp_hdr *rte_udp_hdr;
  struct fivetuple_v4 fivetuple;
  uint64_t timestamp = rte_rdtsc();
  uint32_t pktlen;
  int i;
  for (i = 0; i < nb_bufs; i++)
    {
    buf = bufs[i];
    memset(&fivetuple, 0, sizeof(struct fivetuple_v4));
    pktlen = rte_pktmbuf_pkt_len(buf);
    eth_hdr = rte_pktmbuf_mtod(buf, struct rte_ether_hdr *);
    if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
      {
      rte_ipv4_hdr = (struct rte_ipv4_hdr *) &eth_hdr[1];
      fivetuple.src_ip = rte_be_to_cpu_32(rte_ipv4_hdr->src_addr);
      fivetuple.dst_ip = rte_be_to_cpu_32(rte_ipv4_hdr->dst_addr);
      fivetuple.proto = rte_ipv4_hdr->next_proto_id;
      fivetuple.dir = dir;
      switch (rte_ipv4_hdr->next_proto_id)
        {
        case IPPROTO_TCP:
          rte_tcp_hdr = (struct rte_tcp_hdr *) &rte_ipv4_hdr[1];
          fivetuple.src_port = rte_be_to_cpu_16(rte_tcp_hdr->src_port);
          fivetuple.dst_port = rte_be_to_cpu_16(rte_tcp_hdr->dst_port);
        break;

        case IPPROTO_UDP:
          rte_udp_hdr = (struct rte_udp_hdr *) &rte_ipv4_hdr[1];
          fivetuple.src_port = rte_be_to_cpu_16(rte_udp_hdr->src_port);
          fivetuple.dst_port = rte_be_to_cpu_16(rte_udp_hdr->dst_port);
        break;

        case IPPROTO_IGMP:
        case IPPROTO_OSPFIGP:
        break;

        default:
          KERR("IPV4 IPPROTO OTH %d %d", rte_ipv4_hdr->next_proto_id, dir);
        break;
        }
      update_flow_entry(dir, timestamp, pktlen, &fivetuple);
      }
    else if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6))
      {
      }
    else if (eth_hdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
      {
      }
    else
      {
      KERR("UNKNOWN %d %d", dir, eth_hdr->ether_type);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void flow_tab_loop(void)
{
  lws_service(g_wss_context, 0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void flow_tab_init(int flow_mon_port, int flow_table_size)
{
  int i;
  char tabnm[MAX_NAME_LEN];
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  for (i=0; i<2; i++)
    {
    memset(tabnm, 0, MAX_NAME_LEN);
    snprintf(tabnm, MAX_NAME_LEN-1, "flow_table_v4_%d", i);
    g_flow_table_v4[i] = create_flow_table(tabnm, flow_table_size);
    if (g_flow_table_v4[i] == NULL)
      rte_exit(EXIT_FAILURE, "Cannot create flow_table_v4\n");
    }
  memset(g_wss_protocol, 0, 3 * sizeof(struct lws_protocols));
  g_wss_protocol[0].name = "http-only";
  g_wss_protocol[0].callback = callback_http;
  g_wss_protocol[1].name = "flow-export-protocol";
  g_wss_protocol[1].callback = callback_flowexp;
  g_wss_protocol[1].per_session_data_size = sizeof(struct user_data);
  g_wss_protocol[1].rx_buffer_size = 10;
  g_wss_protocol[1].id = 0;
  info.port = flow_mon_port;
  info.iface = NULL;
  info.protocols = g_wss_protocol;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.gid = -1;
  info.uid = -1;
  info.max_http_header_pool = 1;
  info.options = 0;
  g_wss_context = lws_create_context(&info);
  if (g_wss_context == NULL)
    rte_exit(EXIT_FAILURE, "Cannot create lws_context\n");
  KERR("Created g_wss_context\n");
}
/*---------------------------------------------------------------------------*/

