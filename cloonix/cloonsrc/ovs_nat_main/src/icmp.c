/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rxtx.h"
#include "utils.h"



struct icmphdr {
  __u8          type;
  __u8          code;
  __sum16       checksum;
  union {
        struct {
                __be16  id;
                __be16  sequence;
        } echo;
        __be32  gateway;
        struct {
                __be16  __unused;
                __be16  mtu;
        } frag;
        __u8    reserved[4];
  } un;
};


typedef struct t_icmp
{ 
  uint32_t ipdst;
  uint16_t seq;
  uint16_t ident;
  uint32_t count;
  int len_icmp;
  uint8_t *buf_icmp;
  struct t_icmp *prev;
  struct t_icmp *next;
} t_icmp;

static uint32_t g_our_cisco_ip;
static uint32_t g_our_gw_ip;
static uint32_t g_our_dns_ip;
static t_icmp *g_head_icmp;

char *get_nat_name(void);

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
                       int len_icmp, uint8_t *buf_icmp)
{
  t_icmp *cur = (t_icmp *) utils_malloc(sizeof(t_icmp));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
  memset(cur, 0, sizeof(t_icmp));
  cur->ipdst = ipdst;
  cur->seq = seq;
  cur->ident = ident;
  cur->len_icmp = len_icmp;
  cur->buf_icmp = buf_icmp;
  if (g_head_icmp)
    g_head_icmp->prev = cur;
  cur->next = g_head_icmp;
  g_head_icmp = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void icmp_free(t_icmp *cur)
{
  utils_free(cur->buf_icmp);
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
static void timeout_beat(void *data)
{
  t_icmp *next, *cur = g_head_icmp;
  while(cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count > 5)
      icmp_free(cur);
    cur = next;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void icmp_resp(uint32_t ipdst, uint16_t ident, uint16_t seq, int is_ko)
{
  t_icmp *cur = icmp_find(ipdst, ident, seq);
  struct icmphdr icmp_hdr;
  uint16_t chksum;
  uint8_t *data;
  int headip_len = ETHERNET_HEADER_LEN + IPV4_HEADER_LEN;
  int min_len = ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + sizeof(icmp_hdr);

  if (!cur)
    KERR("ERROR, %X", ipdst);
  else
    {
    if (is_ko == 0)
      {
      if (cur->len_icmp < min_len)
        KOUT("ERROR");
      data = cur->buf_icmp + ETHERNET_HEADER_LEN + IPV4_HEADER_LEN;
      ((struct icmphdr *) data)->checksum = 0;
      chksum = icmp_chksum((uint16_t *) data, cur->len_icmp - headip_len);
      ((struct icmphdr *) data)->checksum = chksum;

      if ((cur->len_icmp == 0) ||
          (cur->len_icmp > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
        KERR("ERROR LEN  %d", cur->len_icmp);
      else
        rxtx_tx_enqueue(cur->len_icmp, cur->buf_icmp);
      }
    icmp_free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void icmp_input(int len, uint8_t *buf)
{
  t_icmp *cur;
  uint8_t *ipv4  = &(buf[ETHERNET_HEADER_LEN]);
  uint8_t *icmp  = &(ipv4[IPV4_HEADER_LEN]);
  uint16_t icmp_cksum, icmp_ident, icmp_seq_nb;
  uint32_t src_ip_addr, cksum;
  uint8_t tmp_mac[6];
  uint8_t tmp_ip[4];
  struct icmphdr *icmp_h = (struct icmphdr *) icmp;
  int llid_prxy = get_llid_prxy();
  char sig_buf[MAX_ICMP_RXTX_SIG_LEN];
  uint8_t *buf_icmp;
  if (icmp[0] != ICMP_ECHO)
    KERR("ERROR ICMP TYPE: %d NOT MANAGED", icmp[0]);
  else
    {
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
      if ((len == 0) ||
          (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
        KERR("ERROR LEN  %d", len);
      else
        rxtx_tx_enqueue(len, buf);
      }
    else if (!msg_exist_channel(llid_prxy))
      {
      KERR("ERROR");
      }
    else
      {
      cur = icmp_find(src_ip_addr, icmp_ident, icmp_seq_nb);
      if (cur)
        KERR("ERROR, %hhu.%hhu.%hhu.%hhu",
             ipv4[12], ipv4[13], ipv4[14], ipv4[15]);
      else
        {
        memset(sig_buf, 0, MAX_ICMP_RXTX_SIG_LEN);
        snprintf(sig_buf, MAX_ICMP_RXTX_SIG_LEN-1,
        "nat_proxy_icmp_req %s ipdst:%X ident:%hu seq:%hu",
        get_nat_name(), src_ip_addr, icmp_ident, icmp_seq_nb);
        rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
        buf_icmp = (uint8_t *) utils_malloc(len);
        memcpy(buf_icmp, buf, len);
        icmp_alloc(src_ip_addr, icmp_ident, icmp_seq_nb, len, buf_icmp);
        }
      }
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
