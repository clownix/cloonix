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
#include "tcp.h"

typedef struct t_llid_ctx
{
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  int count;
  int fd;
  int llid;
} t_llid_ctx;

static uint32_t g_our_gw_ip;
static uint32_t g_host_local_ip;

static uint32_t g_offset;

/****************************************************************************/
static int rx_cb(void *ptr, int llid, int fd)
{
  int data_len, result = 0;
  uint8_t *data;
  data = (uint8_t *) utils_malloc(MAX_RXTX_LEN);
  if (data == NULL)
    KERR(" ");
  else
    {
    data_len = read(fd, data, MAX_RXTX_LEN - g_offset - 4);
    if (data_len == 0)
      {
      utils_free(data);
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      tcp_rx_err(llid);
      }
    else if (data_len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        if (msg_exist_channel(llid))
          msg_delete_channel(llid);
        tcp_rx_err(llid);
        }
      utils_free(data);
      }
    else
      {
      tcp_rx_from_llid(llid, data_len, data);
      result = data_len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(void *ptr, int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  tcp_rx_err(llid);
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
static int open_connect_tcp_sock(uint32_t ip_dst, uint16_t port_dst)
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
    addr.sin_addr.s_addr = htonl(ip_dst);
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
  t_llid_ctx *ctx = (t_llid_ctx *) data;
  int lid, delay;
  if (quick_select_ok(ctx->fd))
    {
    lid = msg_watch_fd(ctx->fd, rx_cb, err_cb, "tcpnat");
    if (lid == 0)
      {
      KERR("%X %X %hu %hu", ctx->sip, ctx->dip, ctx->sport, ctx->dport);
      tcp_connect_resp(ctx->sip, ctx->dip, ctx->sport, ctx->dport, 0, -1);
      }
    else
      {
      tcp_connect_resp(ctx->sip, ctx->dip, ctx->sport, ctx->dport, lid, 0);
      }
    utils_free(ctx);
    }
  else
    {
    close(ctx->fd);
    ctx->count += 1;
    if (ctx->count == 200)
      {
      KERR("%X %X %hu %hu", ctx->sip, ctx->dip, ctx->sport, ctx->dport);
      tcp_connect_resp(ctx->sip, ctx->dip, ctx->sport, ctx->dport, 0, -1);
      utils_free(ctx);
      }
    else
      {
      ctx->fd = open_connect_tcp_sock(ctx->dip, ctx->dport);
      if (ctx->fd < 0)
        {
        KERR("%X %X %hu %hu", ctx->sip, ctx->dip, ctx->sport, ctx->dport);
        tcp_connect_resp(ctx->sip, ctx->dip, ctx->sport, ctx->dport, 0, -1);
        utils_free(ctx);
        }
      else
        {
        delay = 1+(ctx->count/10);
        clownix_timeout_add(delay,timeout_connect,(void *)ctx,NULL,NULL);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void tcp_llid_transmit(int llid, int len, uint8_t *data)
{
  int result;
  if (msg_exist_channel(llid))
    {
    watch_tx(llid, len, (char *) data);
    result = msg_mngt_get_tx_queue_len(llid);
    if (result > 50000)
      KERR("QUEUE TRANSMIT   llid:%d qlen:%d", llid, result);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void tcp_llid_connect(uint32_t sip, uint32_t dip,
                      uint16_t sport, uint16_t dport)
{
  t_llid_ctx *ctx = (t_llid_ctx *) utils_malloc(sizeof(t_llid_ctx)); 
  int fd;
  if (dip == g_our_gw_ip)
    fd = open_connect_tcp_sock(g_host_local_ip, dport);
  else
    fd = open_connect_tcp_sock(dip, dport);
  if (fd < 0)
    {
    KERR("%X %X %d %d", sip, dip, sport & 0xFFFF, dport & 0xFFFF);
    tcp_connect_resp(sip, dip, sport, dport, 0, -1);
    }
  else
    {
    ctx = (t_llid_ctx *) utils_malloc(sizeof(t_llid_ctx)); 
    memset(ctx, 0, sizeof(t_llid_ctx));
    ctx->sip   = sip;
    ctx->dip   = dip;
    ctx->sport = sport;
    ctx->dport = dport;
    ctx->fd = fd;
    clownix_timeout_add(1, timeout_connect, (void *) ctx, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void tcp_llid_init(void)
{
  if (ip_string_to_int (&g_host_local_ip, "127.0.0.1"))
    KOUT(" ");
  g_our_gw_ip    = utils_get_gw_ip();
  g_offset  = sizeof(struct rte_ether_hdr);
  g_offset += sizeof(struct rte_ipv4_hdr);
  g_offset += sizeof(struct rte_tcp_hdr);
}
/*--------------------------------------------------------------------------*/

