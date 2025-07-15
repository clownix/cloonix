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
#include <arpa/inet.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "nat_main.h"
#include "nat_tcp.h"
#include "nat_utils.h"

typedef struct t_tcpcon
{
  char nat[MAX_NAME_LEN];
  int fd;
  int count;
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
} t_tcpcon;

static t_llid_tcp *g_head_llid_tcp;

static uint32_t g_our_gw_ip;
static uint32_t g_host_local_ip;
static uint32_t g_offset;

char *get_proxyshare(void);

/****************************************************************************/
static void tcp_alloc(t_ctx_nat *ctx, int llid, char *stream, uint32_t sip,
                      uint32_t dip, uint16_t sport, uint16_t dport)
{
 t_llid_tcp *cur_llid = (t_llid_tcp *) malloc(sizeof(t_llid_tcp));
  t_tcp_flow *cur = (t_tcp_flow *) malloc(sizeof(t_tcp_flow));
  memset(cur_llid, 0, sizeof(t_llid_tcp));
  memset(cur, 0, sizeof(t_tcp_flow));
  cur->ctx   = ctx;
  cur->sip   = sip;
  cur->dip   = dip;
  cur->sport = sport;
  cur->dport = dport;
  cur->llid_unix = llid;
  strncpy(cur->stream, stream, MAX_PATH_LEN-1);
  if (ctx->head_tcp)
    ctx->head_tcp->prev = cur;
  cur->next = ctx->head_tcp;
  ctx->head_tcp = cur;
  cur_llid->item = cur;
  if (g_head_llid_tcp)
    g_head_llid_tcp->prev = cur_llid;
  cur_llid->next = g_head_llid_tcp;
  g_head_llid_tcp = cur_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tcp_free(t_ctx_nat *ctx, t_llid_tcp *cur_llid)
{
  t_tcp_flow *cur;
  if ((!ctx) || (!cur_llid))
    KOUT("ERROR %p %p", ctx, cur_llid);
  cur = cur_llid->item;
  if (!cur)
    KOUT("ERROR");

  if ((cur->llid_inet) && (msg_exist_channel(cur->llid_inet)))
    msg_delete_channel(cur->llid_inet);
  if ((cur->llid_unix) && (msg_exist_channel(cur->llid_unix)))
    msg_delete_channel(cur->llid_unix);

 if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == ctx->head_tcp)
    ctx->head_tcp = cur->next;

  if (cur_llid->prev)
    cur_llid->prev->next = cur_llid->next;
  if (cur_llid->next)
    cur_llid->next->prev = cur_llid->prev;
  if (cur_llid == g_head_llid_tcp)
    g_head_llid_tcp = cur_llid->next;

  free(cur);
  free(cur_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_tcp *find_inet_llid(int llid)
{   
 t_llid_tcp *cur_llid = g_head_llid_tcp;
  while(cur_llid)
    {
    if (!cur_llid->item)
      KOUT("ERROR");
    if (llid && (cur_llid->item->llid_inet == llid))
      break;
    cur_llid = cur_llid->next;
    } 
  return cur_llid;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_tcp *find_unix_llid(int llid)
{
 t_llid_tcp *cur_llid = g_head_llid_tcp;
  while(cur_llid)
    {
    if (!cur_llid->item)
      KOUT("ERROR");
    if (llid && (cur_llid->item->llid_unix == llid))
      break;
    cur_llid = cur_llid->next;
    }
  return cur_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_tcp_flow *find_cnx_with_params(t_ctx_nat *ctx,
                                        uint32_t sip, uint32_t dip,
                                        uint16_t sport, uint16_t dport)
{
  t_tcp_flow *cur = ctx->head_tcp;
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
static void no_timeout_tcp_ctx_err(t_tcp_flow *cur)
{       
  int llid_prxy;
  t_llid_tcp *cur_llid = g_head_llid_tcp;
  char sig_buf[2*MAX_PATH_LEN];
  while(cur_llid)
    {
    if (cur_llid->item == cur)
      break;
    cur_llid = cur_llid->next;
    }
  if (cur_llid)
    { 
    llid_prxy = cur->ctx->llid_sig;
    if ((llid_prxy == 0) || (!msg_exist_channel(llid_prxy)))
      KERR("ERROR %s", cur->ctx->nat);
    else
      {
      memset(sig_buf, 0, 2*MAX_PATH_LEN);
      snprintf(sig_buf, 2*MAX_PATH_LEN-1,
      "nat_proxy_tcp_fatal_error %s sip:%X dip:%X sport:%hu dport:%hu",
      cur->ctx->nat, cur->sip, cur->dip, cur->sport, cur->dport);
      rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
      }
    tcp_free(cur->ctx, cur_llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_tcp_ctx_err(void *data)
{
  t_tcp_flow *cur = (t_tcp_flow *) data;
  no_timeout_tcp_ctx_err(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tcp_ctx_err(t_tcp_flow *cur)
{
  if (!cur)
    KERR("ERROR");
  else 
    {
    clownix_timeout_add(500, timeout_tcp_ctx_err, (void *) cur, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_llid_tcp *next_llid, *cur_llid = g_head_llid_tcp;
  t_tcp_flow *cur;
  while (cur_llid)
    {
    next_llid = cur_llid->next;
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR");
    else
      {
      if (cur->delayed_count)
        {
        cur->delayed_count -= 1; 
        if (cur->delayed_count <= 0)
          {
          KERR("ERROR TIMEOUT TCP %s sip:%X dip:%X sport:%hu dport:%hu",
               cur->ctx->nat, cur->sip, cur->dip, cur->sport, cur->dport);
          tcp_ctx_err(cur);
          }
        }
      cur->inactivity_count += 1;
      if (cur->inactivity_count > 100000)
        {
        KERR("ERROR TIMEOUT TCP %s sip:%X dip:%X sport:%hu dport:%hu",
             cur->ctx->nat, cur->sip, cur->dip, cur->sport, cur->dport);
        tcp_ctx_err(cur);
        }
      }
    cur_llid = next_llid;
    }
  clownix_timeout_add(1, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void empty_and_clean(void *data)
{
  t_tcp_flow *input_cur = (t_tcp_flow *) data;
  t_llid_tcp *cur_llid = g_head_llid_tcp;
  int result;

  while(cur_llid)
    {
    if (cur_llid->item == input_cur)
      break;
    cur_llid = cur_llid->next;
    }
  if (!cur_llid)
    KERR("ERROR");
  else
    {
    if ((input_cur->llid_inet) &&
        (msg_exist_channel(input_cur->llid_inet)))
      msg_delete_channel(input_cur->llid_inet);
    result = msg_mngt_get_tx_queue_len(input_cur->llid_unix);
    if (result)
      KERR("ERROR QUEUE %d %d", result, input_cur->llid_unix);
    else
      tcp_free(input_cur->ctx, cur_llid);
    }
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tcp_connect_resp(char *nat, uint32_t sip, uint32_t dip,
                             uint16_t sport, uint16_t dport,
                             int llid_inet)
{
  t_ctx_nat *ctx = get_ctx_nat(nat);
  char sig_buf[2*MAX_PATH_LEN];
  t_tcp_flow *cur;
  int llid_prxy;
  if (!ctx)
    KERR("ERROR %s %X %X %hu %hu", nat, sip, dip, sport, dport);
  else
    {
    cur = find_cnx_with_params(ctx, sip, dip, sport, dport);
    if ((!cur) || (!cur->ctx))
      KERR("ERROR %s %X %X %hu %hu", nat, sip, dip, sport, dport);
    else
      {
      llid_prxy = ctx->llid_sig;
      memset(sig_buf, 0, 2*MAX_PATH_LEN);
      if ((llid_prxy == 0) || (!msg_exist_channel(llid_prxy)))
        KERR("ERROR %s %X %X %hu %hu", nat, sip, dip, sport, dport);
      else if (!cur)
        {
        KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
        snprintf(sig_buf, 2*MAX_PATH_LEN-1,
        "nat_proxy_tcp_resp_ko %s sip:%X dip:%X sport:%hu dport:%hu",
        nat, sip, dip, sport, dport);
        rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
        }
      else if (find_inet_llid(llid_inet))
        KERR("ERROR %s %X %X %hu %hu", nat, sip, dip, sport, dport);
      else
        {
        cur->llid_inet = llid_inet;
        snprintf(sig_buf, 2*MAX_PATH_LEN-1,
        "nat_proxy_tcp_resp_ok %s sip:%X dip:%X sport:%hu dport:%hu",
        nat, sip, dip, sport, dport);
        rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void inet_err_cb(int llid, int err, int from)
{
  t_llid_tcp *cur_llid = find_inet_llid(llid);
  t_tcp_flow *cur;
  if (!cur_llid)
    KERR("ERROR %d %d %d", llid, err, from);
  else
    {
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR %d %d %d", llid, err, from);
    else
      {
      KERR("ERROR %d %d %s", err, from, cur->stream);
      tcp_ctx_err(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/
   
/****************************************************************************/
static void unix_err_cb (int llid, int err, int from)
{   
  t_llid_tcp *cur_llid = find_unix_llid(llid);
  t_tcp_flow *cur;
  if (!cur_llid)
    KERR("ERROR %d %d %d", llid, err, from);
  else
    {
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR %d %d %d", llid, err, from);
    else
      tcp_ctx_err(cur);
    }
  msg_delete_channel(llid);
}   
/*--------------------------------------------------------------------------*/
    
/****************************************************************************/
static void unix_rx_cb (int llid, int len, char *buf)
{   
  int i, nb_pkts, left_over, result;
  t_llid_tcp *cur_llid = find_unix_llid(llid);
  t_tcp_flow *cur;
  int max_len = HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN;

  if (!cur_llid)
    {
    KERR("ERROR %d %d", llid, len);
    msg_delete_channel(llid);
    }
  else
    {
    cur = cur_llid->item;
    if (!cur)
      {
      KERR("ERROR %d %d", llid, len);
      msg_delete_channel(llid);
      }
    else if (cur->llid_unix != llid)
      {
      KERR("ERROR UNIX %d %d %d", llid, cur->llid_unix, cur->llid_inet);
      tcp_ctx_err(cur);
      }
    else if (!msg_exist_channel(cur->llid_inet))
      {
      tcp_ctx_err(cur);
      }
    else
      {
      cur->inactivity_count = 0;
      nb_pkts = len / max_len;
      left_over = len % max_len;
      for (i=0; i<nb_pkts; i++)
        {
        watch_tx(cur->llid_inet, max_len, buf+(i*max_len));
        }
      if (left_over)
        {
        watch_tx(cur->llid_inet, left_over, buf+(nb_pkts*max_len));
        } 
      result = msg_mngt_get_tx_queue_len(cur->llid_inet);
      if (result > 50000)
        KERR("QUEUE TRANSMIT len:%d qlen:%d", len, result);
      if (channel_get_rx_local_flow_ctrl(cur->llid_inet))
        {
        KERR("ERROR RED FLOW STOP RX %d", cur->llid_inet);
        }
      if (channel_get_tx_local_flow_ctrl(cur->llid_inet))
        KERR("ERROR RED FLOW STOP TX %d", cur->llid_inet);
      cur->inet_tx_bytes += len;
      }
    }
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int inet_rx_cb(int llid, int fd)
{
  t_llid_tcp *cur_llid = find_inet_llid(llid);
  t_tcp_flow *cur;
  int data_len, result = 0;
  char data[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
  int max_len = HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN;

  data_len = read(fd, data, max_len - g_offset);

  if (!cur_llid)
    {
    KERR("ERROR %d %d", llid, fd);
    msg_delete_channel(llid);
    }
  else
    {
    cur = cur_llid->item;
    if (!cur)
      {
      KERR("ERROR %d %d", llid, fd);
      msg_delete_channel(llid);
      }
    else if (cur->llid_inet != llid)
      {
      KERR("ERROR INET %d %d %d", llid, cur->llid_unix, cur->llid_inet);
      tcp_ctx_err(cur);
      }
    else if (data_len == 0)
      {
      if (cur->empty_and_clean_called == 0)
        {
        channel_rx_local_flow_ctrl(cur->llid_inet, 1);
        cur->empty_and_clean_called = 1;
        if ((cur->llid_inet) &&
            (msg_exist_channel(cur->llid_inet)))
          msg_delete_channel(cur->llid_inet);
    //    clownix_timeout_add(20, empty_and_clean, (void *) cur, NULL, NULL);
        }
      }
    else if (data_len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR) && (errno != EINPROGRESS))
        {
        KERR("ERROR INET %d %s", errno, cur->stream);
        tcp_ctx_err(cur);
        }
      }
    else if (!msg_exist_channel(cur->llid_unix))
      {
      KERR("ERROR INET TO UNIX %s", cur->stream);
      no_timeout_tcp_ctx_err(cur);
      }
    else
      {
      cur->inet_rx_bytes += data_len;
      if (channel_get_rx_local_flow_ctrl(cur->llid_unix))
        KERR("ERROR RED FLOW STOP RX %d", cur->llid_unix);
      if (channel_get_tx_local_flow_ctrl(cur->llid_unix))
        KERR("ERROR RED FLOW STOP TX %d", cur->llid_unix);
      cur->inactivity_count = 0;
      proxy_traf_unix_tx(cur->llid_unix, data_len, data);
      result = data_len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int quick_select_ok(int fd)
{
  int result = 0;
  fd_set fdset;
  struct timeval tv;
  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if (select(fd + 1, NULL, &fdset, NULL, &tv) == 1)
    {
    if (fcntl(fd, F_GETFD) != -1)
      result = 1;
    else
      KERR("POSSIBLE?NO");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int open_connect_tcp_sock(uint32_t ipdst, uint16_t port_dst)
{
  int opt=1, fd, len;
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd >= 0)
    {
    nonblock_fd(fd);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt ));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(ipdst);
    addr.sin_port = htons(port_dst);
    len = sizeof (struct sockaddr_in);
    connect(fd, (const struct sockaddr *) &addr, len);
    }
  return fd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_connect(void *data)
{
  t_tcpcon *con = (t_tcpcon *) data;
  int llid, delay;
  if (quick_select_ok(con->fd))
    {
    llid = msg_watch_fd(con->fd, inet_rx_cb, inet_err_cb, "tcpnat");
    if (llid == 0)
      {
      KERR("ERROR %X %X %hu %hu",con->sip,con->dip,con->sport,con->dport);
      tcp_connect_resp(con->nat,con->sip,con->dip,con->sport,con->dport,0);
      }
    else
      {
      tcp_connect_resp(con->nat,con->sip,con->dip,con->sport,con->dport,llid);
      }
    free(con);
    }
  else
    {
    close(con->fd);
    con->count += 1;
    if (con->count == 200)
      {
      KERR("ERROR %X %X %hu %hu",con->sip,con->dip,con->sport,con->dport);
      tcp_connect_resp(con->nat,con->sip,con->dip,con->sport,con->dport,0);
      free(con);
      }
    else
      {
      if (con->dip == g_our_gw_ip)
        con->fd = open_connect_tcp_sock(g_host_local_ip, con->dport);
      else
        con->fd = open_connect_tcp_sock(con->dip, con->dport);
      if (con->fd < 0)
        {
        KERR("ERROR %X %X %hu %hu",con->sip,con->dip,con->sport,con->dport);
        tcp_connect_resp(con->nat,con->sip,con->dip,con->sport,con->dport,0);
        free(con);
        }
      else
        {
        delay = 1+(con->count/10);
        clownix_timeout_add(delay,timeout_connect,(void *)con,NULL,NULL);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void tcp_connect(char *nat, uint32_t sip, uint32_t dip,
                        uint16_t sport, uint16_t dport)
{
  t_tcpcon *con;
  int fd;
  if ((!nat) || (!strlen(nat)))
    KOUT("ERROR");
  con = (t_tcpcon *) malloc(sizeof(t_tcpcon));
  if (dip == g_our_gw_ip)
    fd = open_connect_tcp_sock(g_host_local_ip, dport);
  else
    fd = open_connect_tcp_sock(dip, dport);
  if (fd < 0)
    {
    KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
    tcp_connect_resp(nat, sip, dip, sport, dport, 0);
    }
  else
    {
    con = (t_tcpcon *) malloc(sizeof(t_tcpcon));
    memset(con, 0, sizeof(t_tcpcon));
    strncpy(con->nat, nat, MAX_NAME_LEN-1);
    con->sip   = sip;
    con->dip   = dip;
    con->sport = sport;
    con->dport = dport;
    con->fd = fd;
    clownix_timeout_add(1, timeout_connect, (void *) con, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int nat_tcp_stream_proxy_req(t_ctx_nat *ctx, char *end_stream,
                             uint32_t sip, uint32_t dip,
                             uint16_t sport, uint16_t dport)
{
  int llid, result = -1;
  char *proxydir = get_proxyshare();
  char stream[MAX_PATH_LEN];
  if ((!ctx) || (!end_stream) || (!strlen(end_stream)))
    KOUT("ERROR");
  memset(stream, 0, MAX_PATH_LEN);
  snprintf(stream, MAX_PATH_LEN-1, "%s/%s", proxydir, end_stream);
  llid = proxy_traf_unix_client(stream, unix_err_cb, unix_rx_cb);
  if (llid == 0)
    KERR("ERROR %s", stream);
  else if (find_unix_llid(llid))
    KERR("ERROR %s", stream);
  else if (find_inet_llid(llid))
    KERR("ERROR %s", stream);
  else
    {
    tcp_alloc(ctx, llid, stream, sip, dip, sport, dport);
    tcp_connect(ctx->nat, sip, dip, sport, dport);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void nat_tcp_stop_go(t_ctx_nat *ctx, uint32_t sip, uint32_t dip, 
                     uint16_t sport, uint16_t dport, int stop)
{ 
  t_tcp_flow *cur;
  if (!ctx)
    KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
  else
    {
    cur = find_cnx_with_params(ctx, sip, dip, sport, dport);
    if (!cur)
      KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
    else if (stop)
      {
      cur->stopped = 1;
      channel_rx_local_flow_ctrl(cur->llid_inet, 1);
      } 
    else
      {
      cur->stopped = 0;
      channel_rx_local_flow_ctrl(cur->llid_inet, 0);
      } 
    } 
}     
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void nat_tcp_del(t_ctx_nat *ctx)
{
  t_llid_tcp *next_llid, *cur_llid = g_head_llid_tcp;
  t_tcp_flow *cur;
  if (!ctx)
    KERR("ERROR");
  else
    {
    while (cur_llid)
      {
      next_llid = cur_llid->next;
      cur = cur_llid->item;
      if (!cur)
        KERR("ERROR");
      else if (cur->ctx != ctx)
        KERR("ERROR");
      else
        tcp_free(ctx, cur_llid);
      cur_llid = next_llid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void nat_tcp_init(void)
{
  if (ip_string_to_int (&g_host_local_ip, "127.0.0.1"))
    KOUT(" ");
  g_our_gw_ip    = utils_get_gw_ip();
  g_offset  = ETHERNET_HEADER_LEN;
  g_offset += IPV4_HEADER_LEN;
  g_offset += TCP_HEADER_LEN;
  g_head_llid_tcp = NULL;
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

