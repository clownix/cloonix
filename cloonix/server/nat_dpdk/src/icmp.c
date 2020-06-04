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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>

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
#include "txq_dpdk.h"
#include "utils.h"

typedef struct t_icmp
{ 
  uint32_t ipdst;
  uint16_t seq;
  uint16_t ident;
  uint32_t count;
  int llid;
  struct rte_mbuf *mbuf;
  struct t_icmp *prev;
  struct t_icmp *next;
} t_icmp;

static uint32_t g_our_gw_ip;
static uint32_t g_our_dns_ip;
static t_icmp *g_head_icmp;

/****************************************************************************/
static t_icmp *icmp_find(uint32_t ipdst, uint16_t ident, uint16_t seq)
{
  t_icmp *cur = g_head_icmp;
  while(cur)
    {
    if ((cur->ipdst == ipdst) && (cur->ident == ident) && (cur->seq == seq))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void icmp_alloc(uint32_t ipdst, uint16_t ident, uint16_t seq,
                       int llid, struct rte_mbuf *mbuf)
{
  t_icmp *cur = (t_icmp *) malloc(sizeof(t_icmp));
  memset(cur, 0, sizeof(t_icmp));
  cur->ipdst = ipdst;
  cur->seq = seq;
  cur->ident = ident;
  cur->llid = llid;
  cur->mbuf = mbuf;
  if (g_head_icmp)
    g_head_icmp->prev = cur;
  cur->next = g_head_icmp;
  g_head_icmp = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void icmp_free(t_icmp *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_icmp)
    g_head_icmp = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static uint16_t icmp_chksum(uint16_t *icmph, int len)
{
  uint16_t ret = 0;
  uint32_t sum = 0;
  uint16_t odd_byte;
  while (len > 1)
    {
    sum += *icmph++;
    len -= 2;
    }
  if (len == 1)
    {
    *(uint8_t*)(&odd_byte) = * (uint8_t*)icmph;
    sum += odd_byte;
    }
  sum =  (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  ret =  ~sum;
  return ret; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ping_destore(t_icmp *cur, int drop)
{
  if (drop)
    rte_pktmbuf_free(cur->mbuf);
  else
    txq_dpdk_enqueue(cur->mbuf, 0);
  if (cur->llid > 0)
    {
    if (msg_exist_channel(cur->llid))
      msg_delete_channel(cur->llid);
    }
  icmp_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(void *ptr, int llid, int fd)
{
  socklen_t slen;
  struct rte_ipv4_hdr *ipv4_h;
  struct icmphdr *rcv_hdr;
  t_icmp *cur;
  int rc;
  uint8_t data[MAX_RXTX_LEN];
  slen = 0;
  rc = recvfrom(fd, data, MAX_RXTX_LEN, 0, NULL, &slen);
  if (rc <= 0)
    KERR(" ");
  else if (rc < sizeof rcv_hdr)
    KERR("%d", rc);
  else
    {
    ipv4_h  = (struct rte_ipv4_hdr *) data;
    rcv_hdr = (struct icmphdr *) (data + sizeof(struct rte_ipv4_hdr));
    if (rcv_hdr->type != ICMP_ECHOREPLY)
      KERR("%d", rcv_hdr->type);
    else
      {
      cur = icmp_find(ipv4_h->src_addr, rcv_hdr->un.echo.id,
                      rcv_hdr->un.echo.sequence);
      if (!cur)
        {
        KERR("ICMP Reply, %X id=%d, seq=%d", ipv4_h->src_addr,
              (int)rcv_hdr->un.echo.id, (int)rcv_hdr->un.echo.sequence); 
        }
      else
        {
        ping_destore(cur, 0);
        }
      }
    }
  return rc;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_cb (void *ptr, int llid, int err, int from)
{
  KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int ping_store(uint32_t ipdst, uint16_t ident,
                      uint16_t seq, struct rte_mbuf *mbuf)
{
  struct sockaddr_in addr;
  struct icmphdr icmp_hdr;
  t_icmp *cur = icmp_find(ipdst, ident, seq);
  uint8_t data[MAX_RXTX_LEN];
  uint16_t chksum;
  int llid, fd, rc, result = -1;
  if (cur)
    KERR("%X %d", ipdst, seq);
  else
    {
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
      KERR("SOCK_RAW, IPPROTO_ICMP FAIL");
    else
      {
      memset(&addr, 0, sizeof(addr));
      memset(&icmp_hdr, 0, sizeof(icmp_hdr));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = ipdst;
      icmp_hdr.type = ICMP_ECHO;
      icmp_hdr.un.echo.id = ident;
      icmp_hdr.un.echo.sequence = seq;
      memcpy(data, &icmp_hdr, sizeof(icmp_hdr));
      memcpy(data + sizeof(icmp_hdr), "hello cloonix1", 14);
      chksum = icmp_chksum((uint16_t *) data, sizeof(icmp_hdr) + 14);
      ((struct icmphdr *) data)->checksum = chksum;
      rc = sendto(fd, data, sizeof(icmp_hdr) + 14,
                  0, (struct sockaddr*)&addr, sizeof(addr));
      if (rc <= 0)
        {
        KERR("%X", ipdst); 
        close(fd);
        }
      else
        {
        llid = msg_watch_fd(fd, rx_cb, err_cb, "ping");
        if (llid == 0)
          {
          KERR("%X", ipdst); 
          close(fd);
          }
        else
          {
          icmp_alloc(ipdst, ident, seq, llid, mbuf);
          result = 0;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_icmp *next, *cur = g_head_icmp;
  while(cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count > 5)
      ping_destore(cur, 1);
    cur = next;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void icmp_input(struct rte_ether_hdr *eth_h,
                struct rte_ipv4_hdr *ipv4_h,
                struct rte_icmp_hdr *icmp_h,
                struct rte_mbuf *mbuf)
{
  struct rte_ether_addr eth_addr;
  uint32_t ip_addr;
  uint32_t cksum;
  uint16_t seq = icmp_h->icmp_seq_nb;
  uint16_t ident = icmp_h->icmp_ident;

  rte_ether_addr_copy(&eth_h->s_addr, &eth_addr);
  rte_ether_addr_copy(&eth_h->d_addr, &eth_h->s_addr);
  rte_ether_addr_copy(&eth_addr, &eth_h->d_addr);
  ip_addr = ipv4_h->src_addr;
  ipv4_h->src_addr = ipv4_h->dst_addr;
  ipv4_h->dst_addr = ip_addr;
  icmp_h->icmp_type = RTE_IP_ICMP_ECHO_REPLY;
  cksum = ~icmp_h->icmp_cksum & 0xffff;
  cksum += ~htons(RTE_IP_ICMP_ECHO_REQUEST << 8) & 0xffff;
  cksum += htons(RTE_IP_ICMP_ECHO_REPLY << 8);
  cksum = (cksum & 0xffff) + (cksum >> 16);
  cksum = (cksum & 0xffff) + (cksum >> 16);
  icmp_h->icmp_cksum = ~cksum;
  if ((htonl(g_our_gw_ip) == ipv4_h->src_addr) ||
      (htonl(g_our_dns_ip) == ipv4_h->src_addr))
    {
    txq_dpdk_enqueue(mbuf, 0);
    }
  else
    {
    if (ping_store(ipv4_h->src_addr, ident, seq, mbuf))
      rte_pktmbuf_free(mbuf);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void icmp_init(void)
{
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_our_gw_ip    = utils_get_gw_ip();
  g_our_dns_ip   = utils_get_dns_ip();
  g_head_icmp = NULL;
}
/*--------------------------------------------------------------------------*/
