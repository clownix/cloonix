/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>

#include "ioc.h"
#include "rank_mngt.h"
#include "llid_rank.h"



/*****************************************************************************/
static void db_rpct_send_diag_msg(t_all_ctx *all_ctx, int llid,
                                  int tid, char *line)
{
  DOUT((void *) all_ctx, FLAG_HOP_DIAG, "%s", line);
  rpct_send_diag_msg((void *) all_ctx, llid, tid, line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_traf_cb (void *ptr, int llid, int err, int from)
{
  int is_blkd; 
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  llid_rank_traf_disconnect(all_ctx, llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_traf_cb(void *ptr, int llid)
{
  t_blkd *blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int nb_llid;
  int32_t *llid_tab;
  nb_llid = get_llid_traf_tab(all_ctx, llid, &llid_tab);
  blkd = blkd_get_rx(ptr, llid);
  while(blkd)
    {
    if (nb_llid > 0)
      {
      if ((blkd->payload_len > PAYLOAD_BLKD_SIZE) || 
          (blkd->payload_len <=0))
        {
        KERR("%d %d", (int) PAYLOAD_BLKD_SIZE, blkd->payload_len);
        blkd_free(ptr, blkd);
        }
      else
        blkd_put_tx(ptr, nb_llid, llid_tab, blkd);
      }
    else
      blkd_free(ptr, blkd);
    blkd = blkd_get_rx(ptr, llid);
    }
  free(llid_tab);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_init_traf_cb (void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int nb_llid, is_blkd;
  int32_t *llid_tab;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  nb_llid = get_llid_traf_tab(all_ctx, llid, &llid_tab);
  if (!nb_llid)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_init_traf_cb (void *ptr, int llid)
{
  t_blkd *bd = blkd_get_rx(ptr, llid);
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char name[MAX_PATH_LEN];
  int num, tidx;
  if (!bd)
    KERR(" ");
  else
    {
    if (sscanf(bd->payload_blkd, "TRAFFIC_OPENING: name=%s num=%d tidx=%d",
                                  name, &num, &tidx) == 3)
      {
      if (!llid_rank_traf_connect(all_ctx, llid, name, num, tidx))
        blkd_server_set_callbacks((void *) all_ctx, llid, rx_traf_cb,
                                                          err_traf_cb);
      }
    else
      KERR("%s %d", bd->payload_blkd, llid);
    blkd_free(ptr, bd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void traf_connection(void *ptr, int llid, int llidnew)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  blkd_server_set_callbacks(all_ctx, llidnew, rx_init_traf_cb,
                            err_init_traf_cb);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_timeout_self_destroy(t_all_ctx *all_ctx, void *data)
{
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void self_destroy(t_all_ctx *all_ctx)
{
  int is_blkd, i, llid;
  uint32_t max_rank = rank_get_max();
  t_llid_rank *dr;
  char req[MAX_RESP_LEN];
  llid = all_ctx->g_llid_listen_traf;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  all_ctx->g_llid_listen_traf = 0;
  for (i=1; i<max_rank; i++)
    {
    dr = rank_get_with_idx(i);
    if (dr)
      {
      if (msg_exist_channel(all_ctx, dr->llid, &is_blkd, __FUNCTION__))
        {
        sprintf(req,
        "mulan_req_end lan=%s name=%s num=%d tidx=%d rank=%d",
        all_ctx->g_name, dr->name, dr->num, dr->tidx, dr->rank); 
        db_rpct_send_diag_msg(all_ctx, dr->llid, 0, req);
        }
      }
    }
  clownix_timeout_add(all_ctx,100,fct_timeout_self_destroy,NULL,NULL,NULL);
  all_ctx->g_self_destroy_requested = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char resp[MAX_RESP_LEN];
  char sock_path[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int num, tidx;
  memset (resp, 0, MAX_RESP_LEN); 
  snprintf(resp, MAX_RESP_LEN-1, "KO");  
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s", line);
  if (all_ctx->g_self_destroy_requested)
    snprintf(resp, MAX_RESP_LEN-1, "SELF-DESTROYING");  
  else if (!strcmp(line, "cloonix_req_quit"))
    {
    self_destroy(all_ctx);
    snprintf(resp, MAX_RESP_LEN-1, "cloonix_resp_quit");  
    }
  else if (sscanf(line, 
           "muend_req_handshake lan=%s name=%s num=%d tidx=%d", 
           lan, name, &num, &tidx) == 4)
    {
    if (strcmp(lan, all_ctx->g_name))
      KOUT("%s %s", lan, all_ctx->g_name);
    snprintf(resp, MAX_PATH_LEN-1,
             "muend_resp_handshake_ok lan=%s name=%s num=%d tidx=%d",
             lan, name, num, tidx);
    }
  else if (sscanf(line, "cloonix_req_listen=%s lan=%s", sock_path, lan) == 2)
    {
    if (strcmp(lan, all_ctx->g_name))
      KOUT("%s %s", lan, all_ctx->g_name);
    all_ctx->g_llid_listen_traf = 
    blkd_server_listen((void *) all_ctx, sock_path, traf_connection);
    if (all_ctx->g_llid_listen_traf == 0)
      {
      KERR("%s", line);
      snprintf(resp, MAX_RESP_LEN-1, "cloonix_resp_listen_ko=%s", sock_path);
      }
    else
      {
      strncpy(all_ctx->g_listen_traf_path, sock_path, MAX_RESP_LEN-1);
      snprintf(resp, MAX_RESP_LEN-1, "cloonix_resp_listen_ok=%s", sock_path);
      }
    }
  else
    {
    rank_dialog(all_ctx, llid, line, resp);
    }
  if (!strncmp(resp, "KO", 2))
    KERR("%s", resp);
  else
    db_rpct_send_diag_msg(all_ctx, llid, tid, resp);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{ 
  int llid_traf, rank, stop;
  DOUT(ptr, FLAG_HOP_EVT, "%s", line);
  if (sscanf(line,
           "cloonix_evt_peer_flow_control_tx rank=%d stop=%d",
           &rank, &stop) == 2)
    {
    llid_traf = blkd_get_llid_with_rank(ptr, rank);
    if (!llid_traf)
      KERR("%s", line);
    else
      blkd_tx_local_flow_control(ptr, llid_traf, stop);
    }
  else
    KERR("%s", line);
}
/*---------------------------------------------------------------------------*/





