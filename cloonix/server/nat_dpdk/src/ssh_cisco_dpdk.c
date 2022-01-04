/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "txq_dpdk.h"
#include "ssh_cisco_llid.h"
#include "tcp_flagseq.h"

enum {
  state_none = 0,
  state_wait_arp_resp,
  state_running,
};

typedef struct t_arp
{
  uint32_t sip;
  uint32_t dip;
  uint16_t dport;
  uint8_t smac[6];
  char vm[MAX_NAME_LEN];
  int llid;
  int count;
  struct t_arp *prev;
  struct t_arp *next;
} t_arp;


typedef struct t_cnx
{
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t smac[6];
  uint8_t dmac[6];
  int llid;
  int inactivity_count;
  int destruction_count;
  t_flagseq *flagseq;
  struct t_cnx *prev;
  struct t_cnx *next;
} t_cnx;


static t_cnx *g_head_cnx;
static t_arp *g_head_arp;

static uint16_t g_sport;




/****************************************************************************/
static t_arp *find_arp(int dip)
{
  t_arp *arp = g_head_arp;
  while(arp)
    {
    if (arp->dip == dip)
      break;
    arp = arp->next;
    }
  return arp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_arp *alloc_arp(uint32_t dip, uint16_t dport, int llid, char *vm)
{
  t_arp *arp = (t_arp *) utils_malloc(sizeof(t_arp));
  if (arp == NULL)
    KERR(" ");
  else
    {
    memset(arp, 0, sizeof(t_arp));
    arp->dip = dip;
    arp->dport = dport;
    arp->llid = llid;
    strncpy(arp->vm, vm, MAX_NAME_LEN-1);
    if (g_head_arp)
      g_head_arp->prev = arp;
    arp->next = g_head_arp;
    g_head_arp = arp;
    }
  return arp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_arp(t_arp *arp)
{
  if (arp->prev)
    arp->prev->next = arp->next;
  if (arp->next)
    arp->next->prev = arp->prev;
  if (arp == g_head_arp)
    g_head_arp = arp->next;
  utils_free(arp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static uint16_t get_new_sport(void)
{
  g_sport += 1;
  if (g_sport == 60000)
    g_sport = 50001;
  return g_sport;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cnx *find_cnx_with_flow(uint32_t sip, uint32_t dip,
                                 uint16_t sport, uint16_t dport)
{
  t_cnx *cur = g_head_cnx;
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
static t_cnx *find_cnx(int llid)
{
  t_cnx *cur = g_head_cnx;
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cnx *alloc_cnx(int llid, uint32_t sip, uint32_t dip,
                        uint16_t sport, uint16_t dport)
{
  t_cnx *cur = (t_cnx *) utils_malloc(sizeof(t_cnx));
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_cnx));
    cur->llid = llid;
    cur->sip = sip;
    cur->dip = dip;
    cur->sport = sport;
    cur->dport = dport;
    if (g_head_cnx)
      g_head_cnx->prev = cur;
    cur->next = g_head_cnx;
    g_head_cnx = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void unix2inet_ko(t_arp *arp)
{
  char *ack="UNIX2INET_NACK_TIMEOUT";
  int len = strlen(ack) + 1;
  ssh_cisco_llid_transmit(arp->llid, len, ((uint8_t *)ack));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_cnx(t_cnx *cur)
{
  ssh_cisco_llid_close(cur->llid);
  if (cur->flagseq)
    tcp_flagseq_end(cur->flagseq);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_cnx)
    g_head_cnx = cur->next;
  utils_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void request_mac_with_ip(t_arp *arp)
{
  uint8_t smac[6], dmac[6];
  uint32_t sip = utils_get_cisco_ip();
  uint8_t *buf;
  struct rte_mbuf *mbuf;
  int32_t len = sizeof(struct rte_ether_hdr) +
                sizeof(struct rte_arp_hdr)   +
                sizeof(struct rte_arp_ipv4);
  mbuf    = utils_alloc_pktmbuf(len + 4 + EMPTY_HEAD);
  buf     = rte_pktmbuf_mtod(mbuf, uint8_t *);
  memset(buf, 0, EMPTY_HEAD);
  buf     = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, EMPTY_HEAD);
  utils_get_mac("FF:FF:FF:FF:FF:FF", dmac);
  utils_get_mac(NAT_MAC_CISCO, smac);
  arp->sip = sip;
  memcpy(arp->smac, smac, 6);
  format_cisco_arp_req(buf, smac, dmac, sip, arp->dip);
  txq_dpdk_enqueue(mbuf, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_end_of_flow(uint32_t sip, uint32_t dip,
                                uint16_t sport, uint16_t dport, int fast)
{   
  t_cnx *cur = find_cnx_with_flow(dip, sip, dport, sport);
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
      free_cnx(cur);
    else if (fast == 1)
      cur->destruction_count = MAIN_DESTRUCTION_DECREMENTER;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_rx_err(int llid)
{
  t_cnx *cur = find_cnx(llid);
  if (cur)
    {
    if (cur->flagseq)
      {
      tcp_flagseq_llid_was_cutoff(cur->flagseq);
      }
    cur->destruction_count = MAIN_DESTRUCTION_DECREMENTER;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_rx_from_llid(int llid, int data_len, uint8_t *data)
{
  t_cnx *cur = find_cnx(llid);
  if (!cur)
    KERR("%d %d", llid, data_len);
  else
    {
    cur->inactivity_count = 0;
    if (cur->flagseq)
      {
      tcp_flagseq_to_dpdk_data(cur->flagseq, data_len, data);
      }
    else
      KERR("%d %d", llid, data_len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_llid_transmit(int llid, int data_len, uint8_t *data)
{
  t_cnx *cur = find_cnx(llid);
  if (!cur)
    KERR("%d %d", llid, data_len);
  else
    {
    ssh_cisco_llid_transmit(llid, data_len, data);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_arp_resp(uint8_t *dmac, uint32_t dip)
{
  t_arp *arp = find_arp(dip);
  t_cnx *cur;
  uint32_t sip;
  uint16_t sport, dport;
  uint8_t smac[6];
  char vm[MAX_NAME_LEN];
  int llid;
  t_flagseq *flagseq;
  if (!arp)
    KERR("%X", dip);
  else
    {
    memset(vm, 0, MAX_NAME_LEN);
    strncpy(vm, arp->vm, MAX_NAME_LEN-1);
    llid  = arp->llid;
    sip   = arp->sip;
    dport = arp->dport;
    memcpy(smac, arp->smac, 6);
    free_arp(arp);
    sport = get_new_sport();
    if (find_cnx(llid))
      KERR("%X", dip);
    else if (find_cnx_with_flow(sip, dip, sport, dport))
      KERR("%X", dip);
    else
      {
      flagseq = tcp_flagseq_begin(dip,sip,dport,sport,dmac,smac,NULL,1);
      if (flagseq == NULL)
        KERR("%X", dip);
      else
        {
        cur = alloc_cnx(llid, sip, dip, sport, dport);
        if (cur == NULL)
          KERR("%X", dip);
        else
          { 
          memcpy(cur->smac, smac, 6);
          memcpy(cur->dmac, dmac, 6);
          cur->flagseq = flagseq;
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void ssh_cisco_dpdk_syn_ack_ok(uint32_t sip, uint32_t dip,
                                uint16_t sport, uint16_t dport)
{
  char *ack="UNIX2INET_ACK_OPEN";
  int len = strlen(ack) + 1;
  t_cnx *cur = find_cnx_with_flow(dip, sip, dport, sport);
  if (!cur)
    KERR("%X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    ssh_cisco_llid_transmit(cur->llid, len, ((uint8_t *)ack));
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_connect(int llid, char *vm, uint32_t dip, uint16_t dport)
{
  t_arp *arp;
  if ((llid == 0) || (dip == 0))
    KERR("%X %hu", dip, dport);
  else
    {
    arp = find_arp(dip);
    if (arp)
      KERR("%X %hu", dip, dport);
    else
      {
      if (alloc_arp(dip, dport, llid, vm) == NULL)
        KERR("%X %hu", dip, dport);
      }
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_cnx *next, *cur = g_head_cnx;
  t_arp *next_arp, *arp = g_head_arp;
  
  while(arp)
    {
    next_arp = arp->next;
    arp->count += 1;
    if ((arp->count % 100) == 1)
      {
      request_mac_with_ip(arp);
      }
    if (arp->count == 450)
      {
      KERR("%X", arp->dip);
      unix2inet_ko(arp);
      }
    if (arp->count > 455)
      {
      KERR("%X", arp->dip);
      free_arp(arp);
      }
    arp = next_arp;
    }

  while(cur)
    {
    next = cur->next;

    if (cur->flagseq)
      tcp_flagseq_heartbeat(cur->flagseq);

    cur->inactivity_count += 1;
    if (cur->inactivity_count > MAIN_INACTIVITY_COUNT)
      {
      KERR("CLEAN INACTIVITY %X %X %hu %hu", cur->dip, cur->sip,
                                             cur->dport, cur->sport);
      free_cnx(cur);
      }

    if (cur->destruction_count > 0)
      {
      cur->destruction_count -= 1;
      if (cur->destruction_count == 0)
        {
        free_cnx(cur);
        }
      }
    cur = next;
    }
  clownix_timeout_add(1, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ssh_cisco_dpdk_input(uint8_t *smac, uint8_t *dmac,
                         struct rte_ipv4_hdr *ipv4_h,
                         struct rte_tcp_hdr *tcp_h,
                         int data_len, uint8_t *data)
{
  int result = -1;
  uint32_t sip = ntohl(ipv4_h->src_addr);
  uint32_t dip = ntohl(ipv4_h->dst_addr);
  uint16_t sport = ntohs(tcp_h->src_port);
  uint16_t dport = ntohs(tcp_h->dst_port);
  t_cnx *cur = find_cnx_with_flow(dip, sip, dport, sport);
  if (cur)
    {
    result = 0;
    if (cur->flagseq)
      {
      cur->inactivity_count = 0;
      tcp_flagseq_to_llid_data(cur->flagseq,cur->llid,tcp_h,data_len,data);
      }
    else
      {
      KERR("%X %X %hu %hu len:%d", dip, sip, dport, sport, data_len);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ssh_cisco_dpdk_init(void)
{
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_sport = 50000;
  g_head_cnx = NULL;
  g_head_arp = NULL;
}
/*--------------------------------------------------------------------------*/

