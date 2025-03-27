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
#include <sys/un.h>
#include <linux/icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "nat_main.h"
#include "nat_icmp.h"
#include "nat_utils.h"


static t_llid_icmp *g_head_llid_icmp;
  
/****************************************************************************/
static t_icmp_flow *icmp_find(t_ctx_nat *ctx, uint32_t ipdst,
                              uint16_t ident, uint16_t seq)
{
  t_icmp_flow *cur = ctx->head_icmp;
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
static t_llid_icmp *llid_find(int llid)
{ 
  t_llid_icmp *cur = g_head_llid_icmp;
  while(cur) 
    {
    if (!cur->item)
      KOUT("ERROR");
    if (llid && (cur->item->llid == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void icmp_alloc(t_ctx_nat *ctx, int llid, uint32_t ipdst,
                       uint16_t ident, uint16_t seq,
                       struct sockaddr_in *addr)
{
  t_llid_icmp *cur_llid = (t_llid_icmp *) malloc(sizeof(t_llid_icmp));
  t_icmp_flow *cur = (t_icmp_flow *) malloc(sizeof(t_icmp_flow));
  memset(cur_llid, 0, sizeof(t_llid_icmp));
  memset(cur, 0, sizeof(t_icmp_flow));
  cur->ctx = ctx; 
  cur->ipdst = ipdst;
  cur->seq = seq;
  cur->ident = ident;
  cur->llid = llid;
  memcpy(&(cur->addr), addr, sizeof(struct sockaddr_in));
  if (ctx->head_icmp)
    ctx->head_icmp->prev = cur;
  cur->next = ctx->head_icmp;
  ctx->head_icmp = cur;
  cur_llid->item = cur;
  if (g_head_llid_icmp)
    g_head_llid_icmp->prev = cur_llid;
  cur_llid->next = g_head_llid_icmp;
  g_head_llid_icmp = cur_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void icmp_free(t_ctx_nat *ctx, t_llid_icmp *cur_llid)
{
  t_icmp_flow *cur;
  if ((!ctx) || (!cur_llid))
    KOUT("ERROR %p %p", ctx, cur_llid);
  cur = cur_llid->item;
  if (!cur)
    KOUT("ERROR");

  if ((cur->llid) && (msg_exist_channel(cur->llid)))
    msg_delete_channel(cur->llid);

  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == ctx->head_icmp)
    ctx->head_icmp = cur->next;

  if (cur_llid->prev)
    cur_llid->prev->next = cur_llid->next;
  if (cur_llid->next)
    cur_llid->next->prev = cur_llid->prev;
  if (cur_llid == g_head_llid_icmp)
    g_head_llid_icmp = cur_llid->next;

  free(cur);
  free(cur_llid);
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
static void send_back_icmp_resp_ok(t_ctx_nat *ctx, uint32_t ipdst,
                                   uint16_t ident, uint16_t seq)
{
  char sig_buf[2*MAX_PATH_LEN];
  memset(sig_buf, 0, 2*MAX_PATH_LEN);
  snprintf(sig_buf, 2*MAX_PATH_LEN-1,
  "nat_proxy_icmp_resp_ok %s ipdst:%X ident:%hu seq:%hu",
  ctx->nat, ipdst, ident, seq);
  if (ctx->llid_sig == 0)
    KERR("ERROR %X %hd %hd", ipdst, ident, seq);
  else if (!msg_exist_channel(ctx->llid_sig))
    KERR("ERROR %X %hd %hd", ipdst, ident, seq);
  else
    rpct_send_sigdiag_msg(ctx->llid_sig, 0, sig_buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_back_icmp_resp_ko(t_ctx_nat *ctx, uint32_t ipdst,
                                   uint16_t ident, uint16_t seq)
{
  char sig_buf[2*MAX_PATH_LEN];
  memset(sig_buf, 0, 2*MAX_PATH_LEN);
  snprintf(sig_buf, 2*MAX_PATH_LEN-1,
  "nat_proxy_icmp_resp_ko %s ipdst:%X ident:%hu seq:%hu",
  ctx->nat, ipdst, ident, seq);
  if (ctx->llid_sig == 0)
    KERR("ERROR %X %hd %hd", ipdst, ident, seq);
  else if (!msg_exist_channel(ctx->llid_sig))
    KERR("ERROR %X %hd %hd", ipdst, ident, seq);
  else
    rpct_send_sigdiag_msg(ctx->llid_sig, 0, sig_buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
{
  socklen_t slen = sizeof(struct sockaddr_in);
  uint32_t ipdst;
  uint8_t *icmp, icmp_type;
  uint16_t ident, seq;
  int rc;
  uint8_t data[MAX_ICMP_RXTX_LEN];
  t_icmp_flow *cur = NULL;
  t_llid_icmp *cur_llid = llid_find(llid);
  if (!cur_llid)
    {
    KERR("ERROR %d", llid);
    close(fd);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    cur = cur_llid->item;
    }
  if ((!cur) || (cur->llid != llid))
    {
    KERR("ERROR %d", llid);
    close(fd);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    rc = recvfrom(fd, data, MAX_ICMP_RXTX_LEN, 0,
                  (struct sockaddr *) &(cur->addr), &slen);
    if (rc <= 0)
      KERR("ERROR %d %d", rc, errno);
    else if (rc < IPV4_HEADER_LEN + ICMP_HEADER_LEN)
      KERR("ERROR %d", rc);
    else
      {
      ipdst = (data[12]<<24)+(data[13]<<16)+(data[14]<<8)+data[15];
      icmp  = &(data[IPV4_HEADER_LEN]);
      icmp_type  = icmp[0];
      ident = ((icmp[4]<<8) & 0xFF00) + ((icmp[5] & 0xFF));
      seq = ((icmp[6]<<8) & 0xFF00) + ((icmp[7] & 0xFF));
      if (icmp_type == ICMP_ECHOREPLY)
        {
        if (cur->ipdst != ipdst)
          KERR("ERROR  %X %hd %hd", ipdst, ident, seq);
        else if (cur->ident != ident)
          KERR("ERROR  %X %hd %hd", ipdst, ident, seq);
        else if (cur->seq != seq)
          KERR("ERROR  %X %hd %hd", ipdst, ident, seq);
        else
          {
          send_back_icmp_resp_ok(cur->ctx, ipdst, ident, seq);
          icmp_free(cur->ctx, cur_llid);
          }
        }
      else if (icmp_type == ICMP_ECHO)
        {
        }
      else
        KERR("WARNING ICMP TYPE %d RX", icmp_type);
      }
    }
  return rc;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_cb (int llid, int err, int from)
{
  KERR("ERROR %d %d %d", llid, err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_llid_icmp *next_llid, *cur_llid = g_head_llid_icmp;
  t_icmp_flow *cur;  
  while (cur_llid)
    {
    next_llid = cur_llid->next;
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR");
    else
      {
      cur->count += 1;
      if (cur->count > 5)
        {
        KERR("WARNING: ICMP DID NOT RETURN");
        send_back_icmp_resp_ko(cur->ctx, cur->ipdst, cur->ident, cur->seq);
        icmp_free(cur->ctx, cur_llid);
        }
      }
    cur_llid = next_llid;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int ping_emit(uint32_t ipdst, uint16_t ident, uint16_t seq,
                     struct sockaddr_in *addr)
{
  struct icmphdr icmp_hdr;
  uint8_t data[MAX_ICMP_RXTX_LEN];
  uint16_t chksum;
  char *icmpstr="hello cloonix is great";
  int llid, fd, rc, result = 0;

  fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (fd < 0)
    KERR("NEEDS PRIVILEDGES TO OPEN SOCK_RAW AF_INET IPPROTO_ICMP");
  else
    {
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    memset(data, 0, MAX_ICMP_RXTX_LEN);
    icmp_hdr.type = ICMP_ECHO;
    icmp_hdr.un.echo.id = htons(ident);
    icmp_hdr.un.echo.sequence = htons(seq);
    memcpy(data, &icmp_hdr, sizeof(icmp_hdr));
    memcpy(data + sizeof(icmp_hdr), icmpstr, strlen(icmpstr));
    chksum = icmp_chksum((uint16_t *) data, sizeof(icmp_hdr) + strlen(icmpstr));
    ((struct icmphdr *) data)->checksum = chksum;
    rc = sendto(fd, data, sizeof(icmp_hdr) + strlen(icmpstr),
                0, (struct sockaddr*)addr, sizeof(*addr));
    if (rc <= 0)
      {
      KERR("ERROR %X %d", ipdst, seq);
      close(fd);
      }
    else
      {
      llid = msg_watch_fd(fd, rx_cb, err_cb, "ping");
      if (llid == 0)
        {
        KERR("ERROR %X %d", ipdst, seq);
        close(fd);
        }
      else
        {
        result = llid;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_icmp_req(t_ctx_nat *ctx, uint32_t ipdst,
                  uint16_t ident, uint16_t seq)
{
  struct sockaddr_in addr;
  int llid;
  t_icmp_flow *cur = icmp_find(ctx, ipdst, ident, seq);
  if (cur)
    KERR("ERROR %s %X %hd %hd", ctx->nat, ipdst, ident, seq);
  else
    {
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipdst);
    llid = ping_emit(ipdst, ident, seq, &addr);
    if (llid)
      {
      if (llid_find(llid))
        KERR("ERROR %s %X %hd %hd", ctx->nat, ipdst, ident, seq);
      else
        icmp_alloc(ctx, llid, ipdst, ident, seq, &addr);
      }
    else
      send_back_icmp_resp_ko(ctx, ipdst, ident, seq);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_icmp_del(t_ctx_nat *ctx)
{
  t_llid_icmp *next_llid, *cur_llid = g_head_llid_icmp;
  t_icmp_flow *cur;  
  while (cur_llid)
    {
    next_llid = cur_llid->next;
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR");
    else
      icmp_free(ctx, cur_llid);
    cur_llid = next_llid;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_icmp_init(void)
{
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_head_llid_icmp = NULL;
}
/*--------------------------------------------------------------------------*/
