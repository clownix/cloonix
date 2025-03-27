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
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "utils.h"
#include "tcp_flagseq.h"
#include "ssh_cisco_nat.h"


/*--------------------------------------------------------------------------*/
typedef struct t_tcp_flow
{
  char stream[MAX_PATH_LEN];
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t smac[6];
  uint8_t dmac[6];
  int llid_traffic;
  int llid_listen;
  int destruction_count;
  int inactivity_count;
  uint8_t tcp[TCP_HEADER_LEN];
  t_flagseq *flagseq;
  struct t_tcp_flow *prev;
  struct t_tcp_flow *next;
} t_tcp_flow;
/*--------------------------------------------------------------------------*/

static t_tcp_flow *g_head_tcp_flow;

char *get_nat_name(void);
char *get_net_name(void);

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
static t_tcp_flow *find_tcp_flow_llid_traffic(int llid)

{
  t_tcp_flow *cur = g_head_tcp_flow;
  while(cur)
    {
    if ((llid > 0) && (cur->llid_traffic == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_tcp_flow *find_tcp_flow_llid_listen(int llid)

{
  t_tcp_flow *cur = g_head_tcp_flow;
  while(cur)
    {     
    if ((llid > 0) && (cur->llid_listen == llid))
      break;
    cur = cur->next;
    }     
  return cur;
}       
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_tcp_flow *alloc_tcp_flow(int llid_listen, char *stream,
                                  uint32_t sip, uint32_t dip,
                                  uint16_t sport, uint16_t dport,
                                  uint8_t *smac, uint8_t *dmac,
                                  uint8_t *tcp)

{
  t_tcp_flow *cur = NULL;
  cur = (t_tcp_flow *) utils_malloc(sizeof(t_tcp_flow));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
  memset(cur, 0, sizeof(t_tcp_flow));
  cur->sip = sip;
  cur->dip = dip;
  cur->sport = sport;
  cur->dport = dport;
  cur->llid_listen = llid_listen;
  strncpy(cur->stream, stream, MAX_PATH_LEN-1);
  memcpy(cur->smac, smac, 6);
  memcpy(cur->dmac, dmac, 6);
  memcpy(cur->tcp, tcp, TCP_HEADER_LEN);
  if (g_head_tcp_flow)
    g_head_tcp_flow->prev = cur;
  cur->next = g_head_tcp_flow;
  g_head_tcp_flow = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_tcp_flow(t_tcp_flow *cur, int line)
{
  if (cur->llid_listen > 0)
    {
    if (msg_exist_channel(cur->llid_listen))
      msg_delete_channel(cur->llid_listen);
    cur->llid_listen = 0;
    }
  if (cur->llid_traffic > 0)
    {
    if (msg_exist_channel(cur->llid_traffic))
      msg_delete_channel(cur->llid_traffic);
    cur->llid_traffic = 0;
    }
  if (cur->flagseq)
    {
    tcp_flagseq_llid_was_cutoff(cur->flagseq);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_tcp_flow)
    g_head_tcp_flow = cur->next;
  unlink(cur->stream);
  utils_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void destroy_tcp_flow(t_tcp_flow *cur)
{
  if (cur->flagseq)
    tcp_flagseq_end(cur->flagseq);
  free_tcp_flow(cur, __LINE__);
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
static void traf_rx_cb (int llid, int len, char *buf)
{
  t_tcp_flow *cur = find_tcp_flow_llid_traffic(llid);
  if (!cur) 
    {
    KERR("ERROR %d", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    cur->inactivity_count = 0;
    if ((len == 0) ||
        (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
      KERR("ERROR LEN %d", len);
    else
      tcp_flagseq_to_tap(cur->flagseq, len, (uint8_t *) buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void traf_err_cb (int llid, int err, int from)
{
  t_tcp_flow *cur = find_tcp_flow_llid_traffic(llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (cur)
    cur->destruction_count = MAIN_DESTRUCTION_DECREMENTER;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_traf_client(int llid, int llid_new)
{   
  t_tcp_flow *cur;
  msg_mngt_set_callbacks (llid_new, traf_err_cb, traf_rx_cb);
  cur = find_tcp_flow_llid_listen(llid);
  if (!cur)
    KERR("ERROR TCPCONN");
  else
    {
    cur->llid_traffic = llid_new;
    cur->llid_listen = 0;
    }
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int tcpconn_start(char *stream,
                         uint32_t sip, uint32_t dip,
                         uint16_t sport, uint16_t dport)
{
  int llid, result = 0;
  char sig_buf[2*MAX_PATH_LEN];
  int llid_prxy = get_llid_prxy();
  llid = proxy_traf_unix_server(stream, connect_from_traf_client);
  if (llid == 0)
    {
    KERR("ERROR TCPCONN %s", stream);
    }
  else
    {
    memset(sig_buf, 0, 2*MAX_PATH_LEN);
    snprintf(sig_buf, 2*MAX_PATH_LEN-1, 
    "nat_proxy_tcp_req %s stream:%s sip:%X dip:%X sport:%hu dport:%hu",
    get_nat_name(), stream, sip, dip, sport, dport);
    rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
    result = llid;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_end_of_flow(uint32_t sip, uint32_t dip,
                     uint16_t sport, uint16_t dport, int fast)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    if (fast == 2)
      destroy_tcp_flow(cur);
    else if (fast == 1)
      cur->destruction_count = MAIN_DESTRUCTION_DECREMENTER;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_connect_resp(int is_ko, uint32_t sip, uint32_t dip,
                      uint16_t sport, uint16_t dport)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    if ((!cur->llid_traffic) || (cur->flagseq))
      KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
    else if (is_ko)
      {
      destroy_tcp_flow(cur);
      }
    else
      {
      cur->flagseq = tcp_flagseq_begin(sip, dip, sport, dport, cur->smac,
                                       cur->dmac, cur->tcp, 0);
      if (cur->flagseq == NULL)
        {
        KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
        destroy_tcp_flow(cur);
        }
      else
        {
        memset(cur->tcp, 0, TCP_HEADER_LEN);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_input(uint8_t *smac, uint8_t *dmac,
               uint32_t sip, uint32_t dip,
               uint16_t sport, uint16_t dport,
               uint8_t *tcp, int data_len, uint8_t *data)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  uint8_t tcp_flags = tcp[13];
  char sip_ascii[MAX_NAME_LEN];
  char dip_ascii[MAX_NAME_LEN];
  char stream[MAX_PATH_LEN];
  int llid_listen;
  char *net = get_net_name();
  if (!cur)
    {
    if (ssh_cisco_nat_input(smac, dmac, sip, dip, sport, dport,
                            tcp, data_len, data) == -1)
      {
      if (tcp_flags == RTE_TCP_SYN_FLAG)
        {
        if (data_len)
          {
          KERR("ERROR %X %X %hu %hu len:%d",sip,dip,sport,dport,data_len);
          }
        else
          {
          int_to_ip_string (sip, sip_ascii);
          int_to_ip_string (dip, dip_ascii);
          memset(stream, 0, MAX_PATH_LEN-1);
          snprintf(stream, MAX_PATH_LEN-1, "%s_%s/tcp_%s_%hu_%s_%hu.sock",
                   PROXYSHARE_IN, net, sip_ascii, sport, dip_ascii, dport);
          llid_listen = tcpconn_start(stream, sip, dip, sport, dport);
          if (llid_listen == 0)
            KERR("ERROR %X %X %hu %hu len:%d",sip,dip,sport,dport,data_len);
          else
            alloc_tcp_flow(llid_listen, stream, sip, dip,
                           sport, dport, smac, dmac, tcp);
          }
        }
      }
    }
  else
    {
    if (cur->flagseq)
      {
      cur->inactivity_count = 0;
      tcp_flagseq_to_llid_data(cur->flagseq, cur->llid_traffic,
                               tcp, data_len, data);
      }
    else
      {
      if (data_len)
        KERR("ERROR %X %X %hu %hu len:%d %hhu",
             sip, dip, sport, dport, data_len, tcp_flags);
      else
        {
        KERR("WARNING %X %X %hu %hu len:%d %hhu",
             sip, dip, sport, dport, data_len, tcp_flags);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_stream_proxy_resp(int is_ko, char *stream,
                           uint32_t sip, uint32_t dip,
                           uint16_t sport, uint16_t dport)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (!cur)
    {
    KERR("ERROR %s", stream);
    }
  else
    {
    if (is_ko)
      {
      KERR("ERROR %s", stream);
      free_tcp_flow(cur, __LINE__);
      }
    else
      {
      channel_rx_local_flow_ctrl(cur->llid_traffic, 0);
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
    free_tcp_flow(cur, __LINE__);

    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_fatal_error(uint32_t sip,uint32_t dip,uint16_t sport,uint16_t dport)
{
  t_tcp_flow *cur = find_tcp_flow(sip, dip, sport, dport);
  if (cur)
    {        
    destroy_tcp_flow(cur);
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

