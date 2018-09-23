/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <signal.h>

#include "ioc.h"
#include "sock_fd.h"
#include "mueth.h"


/*****************************************************************************/
static void collect_eventfull(t_all_ctx *all_ctx, int tidx,
                              int *nb_pkt_tx, int *nb_bytes_tx,
                              int *nb_pkt_rx, int *nb_bytes_rx)
{
  *nb_pkt_tx = all_ctx->g_traf_endp[tidx].nb_pkt_tx;
  *nb_bytes_tx = all_ctx->g_traf_endp[tidx].nb_bytes_tx;
  *nb_pkt_rx = all_ctx->g_traf_endp[tidx].nb_pkt_rx;
  *nb_bytes_rx = all_ctx->g_traf_endp[tidx].nb_bytes_rx;
  all_ctx->g_traf_endp[tidx].nb_pkt_tx = 0;
  all_ctx->g_traf_endp[tidx].nb_bytes_tx = 0;
  all_ctx->g_traf_endp[tidx].nb_pkt_rx = 0;
  all_ctx->g_traf_endp[tidx].nb_bytes_rx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_endp(t_all_ctx *all_ctx, int cloonix_llid, int tidx)
{
  int nb_pkt_tx, nb_pkt_rx, nb_bytes_tx, nb_bytes_rx;
  char txt[2*MAX_NAME_LEN];
  collect_eventfull(all_ctx, tidx, &nb_pkt_tx, &nb_bytes_tx, 
                                   &nb_pkt_rx, &nb_bytes_rx);
  memset(txt, 0, 2*MAX_NAME_LEN);
  snprintf(txt, (2*MAX_NAME_LEN) - 1, 
           "endp_eventfull_tx_rx %u %d %d %d %d %d",
           (unsigned int) cloonix_get_msec(), tidx, nb_pkt_tx, nb_bytes_tx,
           nb_pkt_rx, nb_bytes_rx);
  rpct_send_evt_msg(all_ctx, cloonix_llid, 0, txt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_can_be_sent(t_all_ctx *all_ctx, void *data)
{
  int i, llid = blkd_get_cloonix_llid((void *) all_ctx);
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      eventfull_endp(all_ctx, llid, i);
      }
    }
  clownix_timeout_add(all_ctx, 5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void main_tx_arrival(t_all_ctx *all_ctx, int tidx, int nb_pkt, int nb_bytes)
{
  all_ctx->g_traf_endp[tidx].nb_pkt_tx += nb_pkt;
  all_ctx->g_traf_endp[tidx].nb_bytes_tx += nb_bytes;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void main_rx_arrival(t_all_ctx *all_ctx, int tidx, int nb_pkt, int nb_bytes)
{
  all_ctx->g_traf_endp[tidx].nb_pkt_rx += nb_pkt;
  all_ctx->g_traf_endp[tidx].nb_bytes_rx += nb_bytes;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_tidx(t_all_ctx *all_ctx, int llid, int line)
{
  int i, tidx = -1;
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    if (llid == all_ctx->g_traf_endp[i].llid_traf)
      {
      tidx = i;
      break;
      }
    }
  if (tidx == -1)
    KOUT("%d", line);
  return tidx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rx_blkd_sock_cb(void *ptr, int llid)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int tidx = get_tidx(all_ctx, llid, __LINE__);
  t_blkd *blkd;
  if (!rx_from_traffic_sock(all_ctx, tidx, NULL))
    {
    blkd = blkd_get_rx((void *) all_ctx, llid);
    while(blkd)
      {
      main_rx_arrival(all_ctx, tidx, 1, blkd->payload_len);
      if (rx_from_traffic_sock(all_ctx, tidx, blkd))
        break;
      blkd = blkd_get_rx((void *) all_ctx, llid);
      }
    }
  else
    {
    blkd = blkd_get_rx((void *) all_ctx, llid);
    if (blkd)
      {
      KERR("RX DROP %s %s %d len:%d", all_ctx->g_net_name, all_ctx->g_name, 
                                      all_ctx->g_num, blkd->payload_len);
      blkd_free((void *) all_ctx, blkd);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_sock_cb(void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int tidx = get_tidx(all_ctx, llid, __LINE__);
  sock_fd_finish(all_ctx, tidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sock_fd_tx(t_all_ctx *all_ctx, t_blkd *blkd)
{ 
  int i, lid, is_blkd, j=0;
  int32_t llid[MAX_TRAF_ENDPOINT];
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    llid[i] = 0;
    lid = all_ctx->g_traf_endp[i].llid_traf;
    if (lid && (msg_exist_channel(all_ctx, lid, &is_blkd, __FUNCTION__)))
      {
      main_tx_arrival(all_ctx, i, 1, blkd->payload_len);
      llid[j] = lid;
      j += 1;
      }
    }
  if (j)
    {
    if ((blkd->payload_len > PAYLOAD_BLKD_SIZE) ||
        (blkd->payload_len <=0))
      {
      KERR("%d %d", (int) PAYLOAD_BLKD_SIZE, blkd->payload_len);
      blkd_free((void *) all_ctx, blkd);
      }
    else
      {
      blkd_put_tx((void *) all_ctx, j, llid, blkd);
      }
    }
  else
    {
    blkd_free((void *) all_ctx, blkd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int sock_fd_open(t_all_ctx *all_ctx, char *lan, int tidx, char *path)
{
  int llid, result = -1;

  if ((tidx>=0) && (tidx < MAX_TRAF_ENDPOINT))
    llid = blkd_client_connect((void *) all_ctx, path, rx_blkd_sock_cb, 
                                                       err_sock_cb);
  else
    KOUT("%d", tidx);
  if (llid <= 0)
    KERR("Bad connection to %s", path);
  else
    {
    all_ctx->g_traf_endp[tidx].llid_traf = llid;
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sock_fd_finish(t_all_ctx *all_ctx, int tidx)
{
  t_traf_endp *traf = &(all_ctx->g_traf_endp[tidx]);
  int is_blkd, cidx, llid;
  llid = traf->llid_traf;
  if (llid)
    {
    cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (cidx)
      msg_delete_channel(all_ctx, llid);
     }
  traf->llid_traf = 0;
  llid = traf->llid_lan;
  if (llid)
    {
    cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (cidx)
      msg_delete_channel(all_ctx, llid);
    }
  traf->llid_lan = 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cloonix_cb(t_all_ctx *all_ctx, int llid, int len, char *buf)
{
  if (rpct_decoder(all_ctx, llid, len, buf))
    {
    KOUT("%s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_err_cb(void *ptr, int llid, int err, int from)
{
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_connect(void *ptr, int llid, int llid_new)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int cloonix_llid = blkd_get_cloonix_llid(ptr);
  if (!cloonix_llid)
    blkd_set_cloonix_llid(ptr, llid_new);
  msg_mngt_set_callbacks (all_ctx, llid_new, cloonix_err_cb, rx_cloonix_cb);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_rpct_heartbeat(t_all_ctx *all_ctx, void *data)
{
  rpct_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_blkd_heartbeat(t_all_ctx *all_ctx, void *data)
{
  blkd_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 1, timeout_blkd_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sock_fd_local_flow_control(t_all_ctx *all_ctx, int stop)
{
  //KERR("TODO FLOW CONTROL");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sock_fd_init(t_all_ctx *all_ctx)
{
  if (all_ctx->qemu_mueth_state)
    pool_tx_init(&(all_ctx->tx_pool));
  if (string_server_unix(all_ctx, all_ctx->g_path, cloonix_connect) == 0)
    KOUT("PROBLEM WITH: %s", all_ctx->g_path);
  clownix_timeout_add(all_ctx, 500, eventfull_can_be_sent, NULL, NULL, NULL);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(all_ctx, 100, timeout_blkd_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

