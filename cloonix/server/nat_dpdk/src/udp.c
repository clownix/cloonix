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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "txq_dpdk.h"

/*--------------------------------------------------------------------------*/
typedef struct t_udp_flow
{
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t *smac;
  uint8_t *dmac;
  int llid;
  int fd;
  int inactivity_count;
  struct t_udp_flow *prev;
  struct t_udp_flow *next;
} t_udp_flow;
/*--------------------------------------------------------------------------*/

static uint32_t g_our_cisco_ip;
static uint32_t g_our_gw_ip;
static uint32_t g_our_dns_ip;
static uint32_t g_host_dns_ip;
static uint32_t g_host_local_ip;
static uint32_t g_offset;
static t_udp_flow *g_head_udp_flow;
/*--------------------------------------------------------------------------*/

static t_udp_flow *find_udp_flow_llid(int llid);
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static uint32_t get_dns_addr(void)
{
  uint32_t dns_addr = 0;
  char dns_ip[MAX_NAME_LEN];
  char buff[512];
  char buff2[256];
  char *ptr_start, *ptr_end;
  FILE *f;
  f = fopen("/etc/resolv.conf", "r");
  if (f)
    {
    while (fgets(buff, 512, f) != NULL)
      {
      if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1)
        {
        ptr_start = buff2 + strspn(buff2, " \r\n\t");
        ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
        if (ptr_end)
          *ptr_end = 0;
        strcpy(dns_ip, ptr_start);
        if (ip_string_to_int (&dns_addr, dns_ip))
          continue;
        else
          break;
        }
      }
    }
  if (!dns_addr)
    {
    dns_addr = g_host_local_ip;
    }
  return dns_addr;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(void *ptr, int llid, int fd)
{
  socklen_t slen;
  int data_len;
  uint8_t buf[MAX_RXTX_LEN];
  uint8_t *data;
  t_udp_flow *cur;
  struct rte_mbuf *mbuf;
  slen = 0;
  data = &(buf[g_offset]);
  data_len = recvfrom(fd, data, MAX_RXTX_LEN - g_offset, 0, NULL, &slen);
  if (data_len <= 0)
    KERR(" ");
  else
    {
    cur = find_udp_flow_llid(llid);
    if (!cur)
      KERR(" ");
    else
      {
      utils_fill_udp_packet(buf, data_len, cur->smac, cur->dmac,
                            cur->sip, cur->dip, cur->sport, cur->dport);
      mbuf = utils_alloc_and_copy_pktmbuf(g_offset + data_len, buf);
      txq_dpdk_enqueue(mbuf, 0);
      }
    }
  return data_len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(void *ptr, int llid, int err, int from)
{
  KERR(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int open_udp_sock(int *ret_fd)
{
  int result = 0;
  struct sockaddr_in addr;
  int fd = socket(AF_INET,SOCK_DGRAM,0);
  if(fd > 0)
    {
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))<0)
      close(fd);
    else
      {
      if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
        KOUT("%d", fd);
      result = msg_watch_fd(fd, rx_cb, err_cb, "udp");
      if (result <= 0)
        KOUT(" ");
      nonblock_fd(fd);
      *ret_fd = fd;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *find_udp_flow(uint32_t sip, uint32_t dip,
                                 uint16_t sport, uint16_t dport)

{
  t_udp_flow *cur = g_head_udp_flow;
  while(cur)
    {
    if ((cur->sip == sip) && (cur->sip == dip) && 
        (cur->sip == sport) && (cur->sip == dport))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *find_udp_flow_llid(int llid)

{
  t_udp_flow *cur = g_head_udp_flow;
  while(cur)
    {
    if ((llid > 0) && (cur->llid == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *alloc_udp_flow(uint32_t sip, uint32_t dip,
                                  uint16_t sport, uint16_t dport,
                                  uint8_t *smac, uint8_t *dmac)

{ 
  t_udp_flow *cur = NULL;
  int fd;
  int llid = open_udp_sock(&fd);
  if (llid)
    {
    cur = (t_udp_flow *) malloc(sizeof(t_udp_flow));
    memset(cur, 0, sizeof(t_udp_flow));
    cur->sip = sip;
    cur->dip = dip;
    cur->sport = sport;
    cur->dport = dport;
    cur->smac = smac;
    cur->dmac = dmac;
    cur->llid = llid;
    cur->fd = fd;
    if (g_head_udp_flow)
      g_head_udp_flow->prev = cur;
    cur->next = g_head_udp_flow;
    g_head_udp_flow = cur;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_udp_flow(t_udp_flow *cur)
{
  
  close(cur->fd);
  if (cur->llid > 0)
    {
    if (msg_exist_channel(cur->llid))
      msg_delete_channel(cur->llid);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_udp_flow)
    g_head_udp_flow = cur->next;
  free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_udp_flow *next, *cur = g_head_udp_flow;
  while(cur)
    {
    next = cur->next;
    cur->inactivity_count += 1;
    if (cur->inactivity_count > 30)
      {
      free_udp_flow(cur);
      }
    cur = next;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void transfert_onto_udp(uint8_t *smac,  uint8_t *dmac,
                               uint32_t sip,   uint32_t dip,
                               uint16_t sport, uint16_t dport,
                               uint32_t wanted_dest_ip,
                               int data_len, uint8_t *data)
{
  struct sockaddr_in addr;
  int ln = sizeof (struct sockaddr);
  t_udp_flow *cur = find_udp_flow(sip, dip, sport, dport);
  if (!cur)
    {
    cur = alloc_udp_flow(dip, sip, dport, sport, dmac, smac);
    }
  if (!cur)
    {
    KERR("%X %X %hu %hu", sip, dip, sport, dport);
    }
  else
    {
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(wanted_dest_ip);
    addr.sin_port = htons(dport);
    sendto(cur->fd, data, data_len, 0, (struct sockaddr *)&addr, ln);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void udp_input(uint8_t *smac, uint8_t *dmac,
               struct rte_ipv4_hdr  *ipv4_h,
               struct rte_udp_hdr   *udp_h,
               int data_len, uint8_t *data)
{
  uint32_t sip = ntohl(ipv4_h->src_addr);
  uint32_t dip = ntohl(ipv4_h->dst_addr);
  uint16_t sport = ntohs(udp_h->src_port);
  uint16_t dport = ntohs(udp_h->dst_port);
  if ((dip & 0xFFFFFF00) == (g_our_gw_ip & 0xFFFFFF00))
    {
    if (dip == g_our_gw_ip)
      {
      if (g_host_dns_ip == g_our_dns_ip)
        transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                           g_our_gw_ip, data_len, data);
      else
        transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                           g_host_local_ip, data_len, data);
      }
    else if (dip == g_our_dns_ip)
      transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                         g_host_dns_ip, data_len, data);
    else if (dip == g_our_cisco_ip)
      KERR("TOOLOOKINTO UDP FROM CISCO %X %X %d %d", sip, dip, sport, dport);
    else
      KERR("%X %X %hu %hu", sip, dip, sport, dport);
    }
  else
    transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                       dip, data_len, data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_init(void)
{
  if (ip_string_to_int (&g_host_local_ip, "127.0.0.1"))
    KOUT(" ");
  g_our_gw_ip      = utils_get_gw_ip();
  g_our_dns_ip     = utils_get_dns_ip();
  g_our_cisco_ip   = utils_get_cisco_ip();
  g_host_dns_ip    = get_dns_addr();
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_head_udp_flow = NULL;
  g_offset = sizeof(struct rte_ether_hdr);
  g_offset += sizeof(struct rte_ipv4_hdr);
  g_offset += sizeof(struct rte_udp_hdr);
}
/*--------------------------------------------------------------------------*/
