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
#include <signal.h>
#include <sys/types.h>
#include <libgen.h>
#include <stdint.h>

#include "ioc.h"
#include "rank_mngt.h"
#include "llid_rank.h"

static int g_spy_llid;
 
/*****************************************************************************/
void linker_helper1_fct(void)
{
  printf("useless");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  DOUT((void *) all_ctx, FLAG_HOP_DIAG, "Hello from lan %s", all_ctx->g_name);
  rpct_hop_print_add_sub((void *) all_ctx, llid, tid, flags_hop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int tid)
{
  rpct_hop_print_del_sub(ptr, llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (strcmp(name, all_ctx->g_name))
    KERR("%s %s", name, all_ctx->g_name);
  if (all_ctx->g_num != num)
    KERR("%s %d %d", name, num, all_ctx->g_num);
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void client_err_cb (void *ptr, int llid, int err, int from)
{
  int is_blkd, cloonix_llid = blkd_get_cloonix_llid(ptr);
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  llid_rank_sig_disconnect(all_ctx, llid);
  if (llid == cloonix_llid)
    exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void client_rx_cb (t_all_ctx *all_ctx, int llid, int len, char *buf)
{
  if (rpct_decoder(all_ctx, llid, len, buf))
    {
    KOUT("%s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void client_connect(void *ptr, int llid, int llid_new)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int cloonix_llid = blkd_get_cloonix_llid(ptr);
  if (!cloonix_llid)
    blkd_set_cloonix_llid(ptr, llid_new);
  msg_mngt_set_callbacks (all_ctx, llid_new, client_err_cb, client_rx_cb);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void spy_err_cb(void *ptr, int llid, int err, int from)
{
  int is_blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  if (g_spy_llid == 0)
    KERR(" ");
  else
    {
    if (llid != g_spy_llid)
      KERR("%d %d", llid, g_spy_llid);
    else  
      {
      traf_chain_extract(llid);
      g_spy_llid = 0;
      KERR(" ");
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void spy_rx_cb(void *ptr, int llid)
{
  t_blkd *blkd;
  blkd = blkd_get_rx(ptr, llid);
  while(blkd)
    {
    KERR("%d", blkd->payload_len);
    blkd_free(ptr, blkd);
    blkd = blkd_get_rx(ptr, llid);
    }
  KERR(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void spy_traf_connection(void *ptr, int llid, int llidnew)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (g_spy_llid == 0)
    {
    if (llidnew <= 0)
      KOUT("%d", llidnew);
    blkd_server_set_callbacks(all_ctx, llidnew, spy_rx_cb, spy_err_cb);
    traf_chain_insert(llidnew);
    g_spy_llid = llidnew;
    KERR(" ");
    }
  else
    KERR(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_lib_mulan(t_all_ctx *all_ctx)
{
  init_rank_mngt();
}
/*---------------------------------------------------------------------------*/

