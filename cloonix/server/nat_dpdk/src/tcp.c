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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tcp_llid.h"
#include "tcp_flagseq.h"
#include "ssh_cisco_dpdk.h"


/*--------------------------------------------------------------------------*/
typedef struct t_tcp_flow
{
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t smac[6];
  uint8_t dmac[6];
  int llid;
  int destruction_count;
  int inactivity_count;
  struct rte_tcp_hdr tcp_hdr;
  t_flagseq *flagseq;
  struct t_tcp_flow *prev;
  struct t_tcp_flow *next;
} t_tcp_flow;
/*--------------------------------------------------------------------------*/

static t_tcp_flow *g_head_tcp_flow;

/****************************************************************************/
static t_tcp_flow *find_tcp_flow(uint32_t sip, uint32_t dip,
                                 uint16_t sport, uint16_t dport)

{
  t_tcp_flow *cur = g_head_tcp_flow;
  while(cur)
    {
    if ((cur->sip == sip) && (cur->dip == dip) &&
        (cur->sport == sport) && (cur->dport == dport))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_tcp_flow *find_tcp_flow_llid(int llid)

{
  t_tcp_flow *cur = g_head_tcp_flow;
  while(cur)
    {
    if ((llid > 0) && (cur->llid == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_tcp_flow *alloc_tcp_flow(uint32_t sip, uint32_t dip,
                                  uint16_t sport, uint16_t dport,
                                  uint8_t *smac, uint8_t *dmac,
                                  struct rte_tcp_hdr *tcp_h)

{
  t_tcp_flow *cur = NULL;
  cur = (t_tcp_flow *) rte_malloc(NULL, sizeof(t_tcp_flow), 0);
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_tcp_flow));
    cur->sip = sip;
    cur->dip = dip;
    cur->sport = sport;
    cur->dport = dport;
    memcpy(cur->smac, smac, 6);
    memcpy(cur->dmac, dmac, 6);
    memcpy(&(cur->tcp_hdr), tcp_h, sizeof(struct rte_tcp_hdr));
    if (g_head_tcp_flow)
      g_head_tcp_flow->prev = cur;
    cur->next = g_head_tcp_flow;
    g_head_tcp_flow = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_tcp_flow(t_tcp_flow *cur)
{
  if (cur->llid > 0)
    {
    if (msg_exist_channel(cur->llid))
      msg_delete_channel(cur->llid);
    cur->llid = 0;
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_tcp_flow)
    g_head_tcp_flow = cur->next;
  rte_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void destroy_tcp_flow(t_tcp_flow *cur)
{
  if (cur->flagseq)
    tcp_flagseq_end(cur->flagseq);
  free_tcp_flow(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_tcp_flow *next, *cur = g_head_tcp_flow;
  while(cur)
    {
    next = cur->next;

    if (cur->flagseq)
      tcp_flagseq_heartbeat(cur->flagseq);

    if (cur->destruction_count > 0)
      {
      cur->destruction_count -= 1;
      if (cur->destruction_count == 0)
        {
        destroy_tcp_flow(cur);
        }
      }

    cur->inactivity_count += 1;
    if (cur->inactivity_count > MAIN_INACTIVITY_COUNT)
      {
      KERR("CLEAN INACTIVITY %X %X %hu %hu", cur->sip, cur->dip,
                                             cur->sport, cur->dport);
      destroy_tcp_flow(cur);
      }

    cur = next;
    }
  clownix_timeout_add(1, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_end_of_flow(uint32_t sip, uint32_t dip,
                     uint16_t sport, uint16_t dport, int fast)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    KERR("%X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    if (cur->llid > 0)
      {
      if (msg_exist_channel(cur->llid))
        msg_delete_channel(cur->llid);
      cur->llid = 0;
      }
    if (fast == 2)
      destroy_tcp_flow(cur);
    else if (fast == 1)
      cur->destruction_count = MAIN_DESTRUCTION_DECREMENTER;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_rx_from_llid(int llid, int data_len, uint8_t *data)
{
  t_tcp_flow *cur = find_tcp_flow_llid(llid);
  if (!cur)
    KERR("%d", llid);
  else
    {
    cur->inactivity_count = 0;
    tcp_flagseq_to_dpdk_data(cur->flagseq, data_len, data);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_rx_err(int llid)
{
  t_tcp_flow *cur = find_tcp_flow_llid(llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!cur)
    KERR("%d", llid);
  else
    {
    if (cur->flagseq)
      {
      tcp_flagseq_llid_was_cutoff(cur->flagseq);
      }
    else
      KERR("TOOLOOKAT %X %X %hu %hu",cur->sip,cur->dip,cur->sport,cur->dport);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_connect_resp(uint32_t sip, uint32_t dip,
                      uint16_t sport, uint16_t dport,
                      int llid, int is_ko)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    KERR("%X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    if ((cur->llid) || (cur->flagseq))
      KERR("%X %X %hu %hu", sip, dip, sport, dport);
    else if (is_ko)
      {
      destroy_tcp_flow(cur);
      }
    else
      {
      cur->llid = llid;
      cur->flagseq = tcp_flagseq_begin(sip, dip, sport, dport, cur->smac,
                                       cur->dmac, &(cur->tcp_hdr), 0);
      if (cur->flagseq == NULL)
        {
        KERR("%X %X %hu %hu", sip, dip, sport, dport);
        destroy_tcp_flow(cur);
        }
      else
        memset(&(cur->tcp_hdr), 0, sizeof(struct rte_tcp_hdr));
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_input(uint8_t *smac, uint8_t *dmac, struct rte_ipv4_hdr *ipv4_h,
               struct rte_tcp_hdr *tcp_h, int data_len, uint8_t *data)
{
  uint32_t sip = ntohl(ipv4_h->src_addr);
  uint32_t dip = ntohl(ipv4_h->dst_addr);
  uint16_t sport = ntohs(tcp_h->src_port);
  uint16_t dport = ntohs(tcp_h->dst_port);
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    {
    if (ssh_cisco_dpdk_input(smac, dmac, ipv4_h, tcp_h, data_len, data) == -1)
      {
      if (tcp_h->tcp_flags == RTE_TCP_SYN_FLAG)
        {
        if (data_len)
          {
          KERR("%X %X %hu %hu len:%d", sip, dip, sport, dport, data_len);
          }
        else
          {
          cur = alloc_tcp_flow(sip, dip, sport, dport, smac, dmac, tcp_h);
          tcp_llid_connect(sip, dip, sport, dport);
          }
        }
      else
        {
        //KERR("%X %X %hu %hu len:%d", sip, dip, sport, dport, data_len);
        }
      }
    }
  else
    {
    if (cur->flagseq)
      {
      cur->inactivity_count = 0;
      tcp_flagseq_to_llid_data(cur->flagseq,cur->llid,tcp_h,data_len,data);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flush_all(void)
{
  t_tcp_flow *next, *cur = g_head_tcp_flow;
  while(cur)
    {
    next = cur->next;
    if (cur->flagseq)
      tcp_flagseq_end(cur->flagseq);
    free_tcp_flow(cur);

    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_init(void)
{
  tcp_flagseq_init();
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_head_tcp_flow = NULL;
}
/*--------------------------------------------------------------------------*/

