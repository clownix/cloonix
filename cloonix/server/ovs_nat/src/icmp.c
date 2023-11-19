/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <linux/icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rxtx.h"
#include "utils.h"

typedef struct t_icmp
{ 
  uint32_t ipdst;
  uint16_t seq;
  uint16_t ident;
  uint32_t count;
  int llid;
  int len;
  uint8_t buf[MAX_RXTX_LEN];
  struct t_icmp *prev;
  struct t_icmp *next;
} t_icmp;

static uint32_t g_our_cisco_ip;
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
                       int llid, int len, uint8_t *buf)
{
  t_icmp *cur = (t_icmp *) utils_malloc(sizeof(t_icmp));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
  memset(cur, 0, sizeof(t_icmp));
  cur->ipdst = ipdst;
  cur->seq = seq;
  cur->ident = ident;
  cur->llid = llid;
  cur->len = len;
  memcpy(cur->buf, buf, len);
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
  utils_free(cur);
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
  if (!drop)
    rxtx_tx_enqueue(cur->len, cur->buf);
  if (cur->llid > 0)
    {
    if (msg_exist_channel(cur->llid))
      msg_delete_channel(cur->llid);
    }
  icmp_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
{
  socklen_t slen;
  uint32_t src_ip_addr;
  uint8_t *icmp, icmp_type;
  uint16_t icmp_ident, icmp_seq_nb;
  t_icmp *cur;
  int rc;
  uint8_t data[MAX_RXTX_LEN];
  slen = 0;
  rc = recvfrom(fd, data, MAX_RXTX_LEN, 0, NULL, &slen);
  if (rc <= 0)
    KERR("ERROR %d %d", rc, errno);
  else if (rc < IPV4_HEADER_LEN + ICMP_HEADER_LEN)
    KERR("ERROR %d", rc);
  else
    {
    src_ip_addr = (data[12]<<24)+(data[13]<<16)+(data[14]<<8)+data[15];
    icmp  = &(data[IPV4_HEADER_LEN]);
    icmp_type  = icmp[0];
    icmp_ident  = (icmp[4]<<8) + icmp[5];
    icmp_seq_nb = (icmp[6]<<8) + icmp[7];
    if (icmp_type != ICMP_ECHOREPLY)
      KERR("ERROR %d", icmp_type);
    else
      {
      cur = icmp_find(src_ip_addr, icmp_ident, icmp_seq_nb);
      if (cur)
        ping_destore(cur, 0);
      }
    }
  return rc;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_cb (int llid, int err, int from)
{
  KERR("ERROR");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int ping_store(uint32_t ipdst, uint16_t ident,
                      uint16_t seq, int len, uint8_t *buf)
{
  struct sockaddr_in addr;
  struct icmphdr icmp_hdr;
  t_icmp *cur = icmp_find(ipdst, ident, seq);
  uint8_t data[MAX_RXTX_LEN];
  uint16_t chksum;
  int llid, fd, rc, result = -1;
  uint8_t ipd[4];

  ipd[0] = (ipdst & 0xFF000000) >> 24;
  ipd[1] = (ipdst & 0x00FF0000) >> 16;
  ipd[2] = (ipdst & 0x0000FF00) >> 8;
  ipd[3] = (ipdst & 0x000000FF) >> 0;

  if (cur)
    KERR("ERROR %hhu.%hhu.%hhu.%hhu %d", ipd[0],ipd[1],ipd[2],ipd[3],seq);
  else
    {
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
      KERR("ERROR %hhu.%hhu.%hhu.%hhu %d", ipd[0],ipd[1],ipd[2],ipd[3],seq);
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
        KERR("ERROR %hhu.%hhu.%hhu.%hhu %d", ipd[0],ipd[1],ipd[2],ipd[3],seq);
        close(fd);
        }
      else
        {
        llid = msg_watch_fd(fd, rx_cb, err_cb, "ping");
        if (llid == 0)
          {
          KERR("ERROR %hhu.%hhu.%hhu.%hhu %d", ipd[0],ipd[1],ipd[2],ipd[3],seq);
          close(fd);
          }
        else
          {
          icmp_alloc(ipdst, ident, seq, llid, len, buf);
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
void icmp_input(int len, uint8_t *buf)
{
  uint8_t *ipv4  = &(buf[ETHERNET_HEADER_LEN]);
  uint8_t *icmp  = &(ipv4[IPV4_HEADER_LEN]);
  uint16_t icmp_cksum, icmp_ident, icmp_seq_nb;
  uint32_t src_ip_addr, cksum;
  uint8_t tmp_mac[6];
  uint8_t tmp_ip[4];
  struct icmphdr *icmp_h = (struct icmphdr *) icmp;

  icmp_cksum  = (icmp[2]<<8) + icmp[3];
  icmp_ident  = (icmp[4]<<8) + icmp[5];
  icmp_seq_nb = (icmp[6]<<8) + icmp[7];
  memcpy(tmp_mac, &(buf[0]), 6);
  memcpy(&(buf[0]), &(buf[6]), 6);
  memcpy(&(buf[6]), tmp_mac, 6);
  memcpy(tmp_ip, &(ipv4[12]), 4);
  memcpy(&(ipv4[12]), &(ipv4[16]), 4);
  memcpy(&(ipv4[16]), tmp_ip, 4);
  icmp[0] = ICMP_ECHOREPLY;
  cksum = ~icmp_cksum & 0xffff;
  cksum += ~htons(ICMP_ECHO << 8) & 0xffff;
  cksum += htons(ICMP_ECHOREPLY << 8);
  cksum = (cksum & 0xffff) + (cksum >> 16);
  cksum = (cksum & 0xffff) + (cksum >> 16);
  icmp_h->checksum = ~cksum;
  src_ip_addr = (ipv4[12]<<24)+(ipv4[13]<<16)+(ipv4[14]<<8)+ipv4[15];
  if ((g_our_cisco_ip == src_ip_addr) ||
      (g_our_gw_ip    == src_ip_addr) ||
      (g_our_dns_ip   == src_ip_addr))
    {
    rxtx_tx_enqueue(len, buf);
    }
  else
    {
    ping_store(src_ip_addr, icmp_ident, icmp_seq_nb, len, buf);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void icmp_init(void)
{
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_our_cisco_ip = utils_get_cisco_ip();
  g_our_gw_ip    = utils_get_gw_ip();
  g_our_dns_ip   = utils_get_dns_ip();
  g_head_icmp = NULL;
}
/*--------------------------------------------------------------------------*/
