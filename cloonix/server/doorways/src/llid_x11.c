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
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "util_sock.h"
#include "sock.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "doorways_sock.h"
#include "local_dropbear.h"



typedef struct t_x11_ctx
{
  int backdoor_llid;
  int dido_llid;
  int sub_dido_idx;
  int display_sock_x11;
  struct t_x11_ctx *prev;
  struct t_x11_ctx *next;

} t_x11_ctx;


static t_x11_ctx *dido_llid_2_ctx[CLOWNIX_MAX_CHANNELS];

/****************************************************************************/
static t_x11_ctx *get_head_ctx_with_dido_llid(int dido_llid)
{
  t_x11_ctx *result;
  if ((dido_llid <= 0) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);
  result = dido_llid_2_ctx[dido_llid];
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_x11_ctx *get_ctx_with_sub_dido_idx(int dido_llid, int sub_dido_idx)
{
  t_x11_ctx *cur = get_head_ctx_with_dido_llid(dido_llid);
  if ((sub_dido_idx < 1) || (sub_dido_idx > MAX_IDX_X11))
    KOUT("%d %d", sub_dido_idx, MAX_IDX_X11);
  while (cur && (cur->sub_dido_idx != sub_dido_idx))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_x11_ctx *alloc_ctx(int backdoor_llid, int dido_llid, 
                            int sub_dido_idx, int display_sock_x11) 
{
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (cur)
    KOUT("%d %d %d", backdoor_llid, dido_llid, sub_dido_idx);
  cur = (t_x11_ctx *) clownix_malloc(sizeof(t_x11_ctx), 10);
  memset(cur, 0, sizeof(t_x11_ctx));
  cur->backdoor_llid = backdoor_llid;
  cur->dido_llid = dido_llid;
  cur->sub_dido_idx = sub_dido_idx;
  cur->display_sock_x11 = display_sock_x11;
  if (dido_llid_2_ctx[dido_llid])
    dido_llid_2_ctx[dido_llid]->prev = cur;
  cur->next = dido_llid_2_ctx[dido_llid];
  dido_llid_2_ctx[dido_llid] = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_ctrl_x11_close_to_client( t_x11_ctx *cur)
{
  char buf[MAX_DOOR_CTRL_LEN];
  memset(buf, 0, MAX_DOOR_CTRL_LEN);
  snprintf(buf, MAX_DOOR_CTRL_LEN-1, DOOR_X11_OPENKO);
  doorways_tx(cur->dido_llid, cur->dido_llid, doors_type_dbssh_x11_ctrl, 
                  cur->sub_dido_idx, strlen(buf) + 1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_ctrl_x11_open_to_client(t_x11_ctx *cur)
{
  int port;
  char buf[MAX_DOOR_CTRL_LEN];
  memset(buf, 0, MAX_DOOR_CTRL_LEN);
  port = llid_traf_get_display_port(cur->dido_llid);
  snprintf(buf,MAX_DOOR_CTRL_LEN-1,DOOR_X11_OPEN,cur->display_sock_x11,port);
  if (doorways_tx(cur->dido_llid, cur->dido_llid, doors_type_dbssh_x11_ctrl,
                  cur->sub_dido_idx, strlen(buf) + 1, buf))
    KERR("%d %d", cur->dido_llid, cur->sub_dido_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_ctx(int dido_llid, int sub_dido_idx)
{
  t_x11_ctx *cur;
  cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (!cur)
    KERR(" ");
  else
    {
    if (msg_exist_channel(cur->backdoor_llid))
      llid_backdoor_tx_x11_close_to_agent(cur->backdoor_llid, 
                                          cur->dido_llid, cur->sub_dido_idx);
    send_ctrl_x11_close_to_client(cur);
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == dido_llid_2_ctx[cur->dido_llid])
      dido_llid_2_ctx[cur->dido_llid] = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_tx_to_agent(t_x11_ctx *ctx, int len, char  *buf)
{
  char *payload;
  int headsize = sock_header_get_size();
  char *start_header = buf - headsize;
  if (len > MAX_A2D_LEN - headsize)
    KOUT("%d", len);
  if (sock_header_get_size() > doorways_header_size())
    KOUT("%d %d", sock_header_get_size(), doorways_header_size());
  sock_header_set_info(start_header, ctx->dido_llid, len, header_type_x11, 
                       ctx->sub_dido_idx, &payload);
  if (payload != buf)
    KOUT("%p %p", payload, buf);
  if (!msg_exist_channel(ctx->backdoor_llid))
    {
    KERR(" ");
    free_ctx(ctx->dido_llid, ctx->sub_dido_idx);
    }
  else
    {
    llid_backdoor_low_tx(ctx->backdoor_llid, len + headsize, start_header);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_open_close (int backdoor_llid, int dido_llid, char *rx_payload)
{
  t_x11_ctx *cur;
  int sub_dido_idx, display_sock_x11;
  DOUT(NULL, FLAG_HOP_DOORS, "%s %d %s", __FUNCTION__, dido_llid, rx_payload);
  if (sscanf(rx_payload, LAX11OPEN, &sub_dido_idx, &display_sock_x11) == 2)
    {
    cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
    if (cur)
      KERR("%d %d", backdoor_llid, dido_llid);
    else
      {
      cur = alloc_ctx(backdoor_llid, dido_llid, sub_dido_idx, display_sock_x11);
      send_ctrl_x11_open_to_client(cur);
      }
    }
  else if (sscanf(rx_payload, LAX11OPENKO, &sub_dido_idx) == 1)
    {
    cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
    if (cur)
      free_ctx(dido_llid, sub_dido_idx);
    }
  else
    KERR("%d %d %s", backdoor_llid, dido_llid, rx_payload); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_close(int backdoor_llid, int dido_llid)
{
  t_x11_ctx *next, *cur = get_head_ctx_with_dido_llid(dido_llid);
  DOUT(NULL, FLAG_HOP_DOORS, "%s %d", __FUNCTION__, dido_llid);
  while (cur)
    {
    if (cur->backdoor_llid != backdoor_llid)
      KOUT("%d %d", cur->backdoor_llid, backdoor_llid);
    next = cur->next;
    free_ctx(cur->dido_llid, cur->sub_dido_idx);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_write(int dido_llid, int sub_dido_idx, int len, char *buf)
{
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (!cur)
    KERR("%d %d", dido_llid, sub_dido_idx);
  else
    {
    if (doorways_tx(dido_llid, dido_llid, doors_type_dbssh_x11_traf,
                    sub_dido_idx, len, buf))
      KERR("%d %d %d", dido_llid, sub_dido_idx, len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void receive_traf_x11_from_client(int dido_llid, int sub_dido_idx,
                                  int len, char *buf)
{
  int  headsize = sock_header_get_size();
  int len_max = MAX_A2D_LEN - headsize;
  int len_done, len_to_do, chosen_len;
  t_x11_ctx *cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (cur)
    {
    len_to_do = len;
    len_done = 0;
    while (len_to_do)
      {
      if (len_to_do >= len_max)
        chosen_len = len_max;
      else
        chosen_len = len_to_do;
      if (cur->backdoor_llid)
        x11_tx_to_agent(cur, chosen_len, buf+len_done);
      else
        local_dropbear_receive_x11_from_client(cur->display_sock_x11, 
                                               sub_dido_idx,
                                               chosen_len, buf+len_done);
      len_done += chosen_len;
      len_to_do -= chosen_len;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void receive_ctrl_x11_from_client(int dido_llid, int sub_dido_idx,
                                  int len, char *buf)
{
  t_x11_ctx *cur;
  DOUT(NULL, FLAG_HOP_DOORS, "%s %d %d %s", __FUNCTION__, 
                                           dido_llid, sub_dido_idx, buf);
  cur = get_ctx_with_sub_dido_idx(dido_llid, sub_dido_idx);
  if (cur)
    {
    if (!strcmp(buf, DOOR_X11_OPENOK))
      {
      if (cur->backdoor_llid)
        {
        llid_backdoor_tx_x11_open_to_agent(cur->backdoor_llid,
                                           dido_llid, sub_dido_idx);
        }
      else
        {
        local_dropbear_x11_open_to_agent(dido_llid, sub_dido_idx);
        } 
      }
    else if (!strcmp(buf, DOOR_X11_OPENKO))
      {
      free_ctx(dido_llid, sub_dido_idx);
      }
    else
      KERR("%d %d %d %s", dido_llid, sub_dido_idx, len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init(void)
{
  memset(dido_llid_2_ctx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_x11_ctx *));
}
/*--------------------------------------------------------------------------*/

