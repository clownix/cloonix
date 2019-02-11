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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ioc_top.h"
#include "rpct_ctx.h"
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_rpct_ctx *g_rpct_ctx;
/*--------------------------------------------------------------------------*/


/****************************************************************************/
t_rpct_ctx *get_rpct_ctx(void *ptr)
{
  t_all_ctx_head *ctx_head;
  t_rpct_ctx *ctx;
  if (!ptr)
    {
    ctx = g_rpct_ctx;
    }
  else
    {
    ctx_head = (t_all_ctx_head *) ptr;
    ctx = (t_rpct_ctx *) ctx_head->rpct_ctx;
    }
  return ctx;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void msg_tx(void *ptr, int llid, int len, char *buf)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  if (ctx->g_string_tx)
    {
    buf[len] = 0;
    ctx->g_string_tx(ptr, llid, len+1, buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_buf_tx(void *ptr)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  return ctx->g_buf_tx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_hop_free_txt(char *msg)
{
  int len;
  char *ptrs, *ptre, *txt;
  ptrs = strstr(msg, "<hop_free_txt_joker>");
  if (!ptrs)
    KOUT("%s", msg);
  ptrs += strlen("<hop_free_txt_joker>");
  ptre = strstr(ptrs, "</hop_free_txt_joker>");
  if (!ptre)
    KOUT("%s", msg);
  len = ptre - ptrs;
  txt = (char *) malloc(len+1);
  memcpy(txt, ptrs, len);
  txt[len] = 0;
  return txt;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_report_sub(void *ptr, int llid, int sub)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (llid)
    {
    len = sprintf(buf, BLKD_ITEM_SUB, sub);
    msg_tx(ptr, llid, len, buf);
    }
  else
    rpct_recv_report_sub(ptr, 0, sub);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_report(void *ptr, int llid, t_blkd_item *item)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!item)
    KOUT(" ");
  if (strlen(item->sock) == 0)
    KOUT(" ");
  if (strlen(item->sock) >= MAX_PATH_LEN)
    KOUT("%s", item->sock);
  if (strlen(item->rank_name) == 0)
    KOUT(" ");
  if (strlen(item->rank_name) >= MAX_NAME_LEN)
    KOUT("%s", item->rank_name);
  len = sprintf(buf, BLKD_ITEM, item->sock, item->rank_name,
                                item->rank_num, item->rank_tidx, 
                                item->rank, item->pid, item->llid, 
                                item->fd, item->sel_tx,
                                item->sel_rx, item->fifo_tx,
                                item->fifo_rx, item->queue_tx,
                                item->queue_rx, item->bandwidth_tx,
                                item->bandwidth_rx, 
                                item->stop_tx, item->stop_rx,
                                item->dist_flow_ctrl_tx, 
                                item->dist_flow_ctrl_rx,
                                item->drop_tx, item->drop_rx);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!name || !strlen(name))
    KOUT(" ");
  len = sprintf(buf, HOP_PID_REQ, tid, name, num);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_kil_req(void *ptr, int llid, int tid)
{
  int len;
  char *buf = get_buf_tx(ptr);
  len = sprintf(buf, HOP_KIL_REQ, tid);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_pid_resp(void *ptr, int llid, int tid,
                        char *name, int num, int toppid, int pid)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!name || !strlen(name))
    KOUT(" ");
  len = sprintf(buf, HOP_PID_RESP, tid, name, num, toppid, pid);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  int len;
  char *buf = get_buf_tx(ptr);
  len = sprintf(buf, HOP_EVT_SUB, tid, flags_hop);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_hop_unsub(void *ptr, int llid, int tid)
{
  int len;
  char *buf = get_buf_tx(ptr);
  len = sprintf(buf, HOP_EVT_UNSUB, tid);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_send_hop_msg(void *ptr, int llid, int tid,
                      int flags_hop, char *txt)
{
  int len;
  char *buf = get_buf_tx(ptr);
  len = sprintf(buf, HOP_EVT_O, tid, flags_hop);
  len += sprintf(buf+len, HOP_FREE_TXT, txt);
  len += sprintf(buf+len, HOP_EVT_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_evt_msg(void *ptr, int llid, int tid, char *line)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUEVT_MSG_O, tid);
  len += sprintf(buf+len, MUEVT_MSG_I, line);
  len += sprintf(buf+len, MUEVT_MSG_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_diag_msg(void *ptr, int llid, int tid, char *line)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUDIAG_MSG_O, tid);
  len += sprintf(buf+len, MUDIAG_MSG_I, line);
  len += sprintf(buf+len, MUDIAG_MSG_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_app_msg(void *ptr, int llid, int tid, char *line)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUAPP_MSG_O, tid);
  len += sprintf(buf+len, MUAPP_MSG_I, line);
  len += sprintf(buf+len, MUAPP_MSG_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_send_cli_req(void *ptr, int llid, int tid,
                        int cli_llid, int cli_tid, char *line)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUCLI_REQ_O, tid, cli_llid, cli_tid);
  len += sprintf(buf+len, MUCLI_REQ_I, line);
  len += sprintf(buf+len, MUCLI_REQ_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_cli_resp(void *ptr, int llid, int tid,
                         int cli_llid, int cli_tid, char *line)
{
  int len;
  char *buf = get_buf_tx(ptr);
  if (!line)
    KOUT(" ");
  if (strlen(line) < 1)
    KOUT(" ");
  if (strlen(line) >= MAX_RPC_MSG_LEN)
    line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, MUCLI_RESP_O, tid, cli_llid, cli_tid);
  len += sprintf(buf+len, MUCLI_RESP_I, line);
  len += sprintf(buf+len, MUCLI_RESP_C);
  msg_tx(ptr, llid, len, buf);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void dispatcher(void *ptr, int llid, int bnd_evt, char *msg)
{
  int sub;
  t_blkd_item item;
  int flags_hop, toppid, pid, tid, len, cli_llid, cli_tid, num;
  char *ptrs, *ptre, *line, *txt;
  char name[MAX_NAME_LEN];


  switch(bnd_evt)
    {

    case bnd_rpct_blkd_item_sub:
      if (sscanf(msg, BLKD_ITEM_SUB, &sub) != 1)
        KOUT("%s", msg);
      rpct_recv_report_sub(ptr, llid, sub);
      break;

    case bnd_rpct_blkd_item:
    memset(&item, 0, sizeof(t_blkd_item));
    if (sscanf(msg, BLKD_ITEM, item.sock, 
                               item.rank_name,
                               &(item.rank_num),
                               &(item.rank_tidx),
                               &(item.rank),
                               &(item.pid),
                               &(item.llid),
                               &(item.fd),
                               &(item.sel_tx),
                               &(item.sel_rx),
                               &(item.fifo_tx),
                               &(item.fifo_rx),
                               &(item.queue_tx),
                               &(item.queue_rx),
                               &(item.bandwidth_tx),
                               &(item.bandwidth_rx),
                               &(item.stop_tx),
                               &(item.stop_rx),
                               &(item.dist_flow_ctrl_tx),
                               &(item.dist_flow_ctrl_rx),
                               &(item.drop_tx),
                               &(item.drop_rx))
                               != 22)
      KOUT("%s", msg);
      rpct_recv_report(ptr, llid, &item);
      break;

    case bnd_rpct_pid_req:
      if (sscanf(msg, HOP_PID_REQ, &tid, name, &num) != 3)
        KOUT("%s", msg);
      rpct_recv_pid_req(ptr, llid, tid, name, num);
      break;

    case bnd_rpct_kil_req:
      if (sscanf(msg, HOP_KIL_REQ, &tid) != 1)
        KOUT("%s", msg);
      rpct_recv_kil_req(ptr, llid, tid);
      break;

    case bnd_rpct_pid_resp:
      if (sscanf(msg, HOP_PID_RESP, &tid, name, &num, &toppid, &pid) != 5)
        KOUT("%s", msg);
      rpct_recv_pid_resp(ptr, llid, tid, name, num, toppid, pid);
      break;

    case bnd_rpct_hop_evt_sub:
      if (sscanf(msg, HOP_EVT_SUB, &tid, &flags_hop) != 2)
        KOUT("%s", msg);
      rpct_recv_hop_sub(ptr, llid, tid, flags_hop);
      break;

    case bnd_rpct_hop_evt_unsub:
      if (sscanf(msg, HOP_EVT_UNSUB, &tid) != 1)
        KOUT("%s", msg);
      rpct_recv_hop_unsub(ptr, llid, tid);
      break;

    case bnd_rpct_hop_evt_msg:
      if (sscanf(msg, HOP_EVT_O, &tid, &flags_hop) != 2)
        KOUT("%s", msg);
      txt = get_hop_free_txt(msg);
      rpct_recv_hop_msg(ptr, llid, tid, flags_hop, txt);
      free(txt);
      break;

    case bnd_rpct_evt_msg:
      if (sscanf(msg, MUEVT_MSG_O, &tid) != 1)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<evt_msg_delimiter>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<evt_msg_delimiter>");
      ptre = strstr(ptrs, "</evt_msg_delimiter>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_evt_msg(ptr, llid, tid, line);
      free(line);
      break;

    case bnd_rpct_app_msg:
      if (sscanf(msg, MUAPP_MSG_O, &tid) != 1)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<app_msg_delimiter>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<app_msg_delimiter>");
      ptre = strstr(ptrs, "</app_msg_delimiter>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_app_msg(ptr, llid, tid, line);
      free(line);
      break;

    case bnd_rpct_diag_msg:
      if (sscanf(msg, MUDIAG_MSG_O, &tid) != 1)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<diag_msg_delimiter>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<diag_msg_delimiter>");
      ptre = strstr(ptrs, "</diag_msg_delimiter>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_diag_msg(ptr, llid, tid, line);
      free(line);
      break;


    case bnd_rpct_cli_req:
      if (sscanf(msg, MUCLI_REQ_O, &tid, &cli_llid, &cli_tid) != 3)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<mucli_req_bound>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<mucli_req_bound>");
      ptre = strstr(ptrs, "</mucli_req_bound>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_cli_req(ptr, llid, tid, cli_llid, cli_tid, line);
      free(line);
      break;

    case bnd_rpct_cli_resp:
      if (sscanf(msg, MUCLI_RESP_O, &tid, &cli_llid, &cli_tid) != 3)
        KOUT("%s", msg);
      ptrs = strstr(msg, "<mucli_resp_bound>");
      if (!ptrs)
        KOUT("%s", msg);
      ptrs += strlen("<mucli_resp_bound>");
      ptre = strstr(ptrs, "</mucli_resp_bound>");
      if (!ptre)
        KOUT("%s", msg);
      len = ptre - ptrs;
      line = (char *) malloc(len+1);
      memset(line, 0, len+1);
      memcpy(line, ptrs, len);
      rpct_recv_cli_resp(ptr, llid, tid, cli_llid, cli_tid, line);
      free(line);
      break;


    default:
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void extract_boundary(char *input, char *output)
{
  int bound_len;
  if (input[0] != '<')
    KOUT("%s\n", input);
  bound_len = strcspn(input, ">");
  if (bound_len >= MAX_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  if (bound_len < MIN_CLOWNIX_BOUND_LEN)
    KOUT("%s\n", input);
  memcpy(output, input, bound_len);
  output[bound_len] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_bnd_evt(void *ptr, char *bound)
{
  int i, result = 0;
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  for (i=bnd_rpct_min; i<bnd_rpct_max; i++)
    {
    if (!strcmp(bound, ctx->g_bound_list[i]))
      {
      result = i;
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rpct_decoder(void *ptr, int llid, int len, char *str_rx)
{
  int  result = -1;
  int  bnd_evt;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len != ((int) strlen(str_rx) + 1))
    KOUT("%d %d %s", len, ((int) strlen(str_rx) + 1), str_rx);
  extract_boundary(str_rx, bound);
  bnd_evt = get_bnd_evt(ptr, bound);
  if (bnd_evt)
    {
    dispatcher(ptr, llid, bnd_evt, str_rx);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_heartbeat(void *ptr)
{
  report_heartbeat(ptr);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_redirect_string_tx(void *ptr, t_rpct_tx rpc_tx)
{
  t_rpct_ctx *ctx = get_rpct_ctx(ptr);
  ctx->g_string_tx = rpc_tx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_send_peer_flow_control(void *ptr, int llid, 
                                 char *name, int num, int tidx, 
                                 int rank, int stop)
{
  char evt[MAX_PATH_LEN];
  snprintf(evt, MAX_PATH_LEN-1,
  "cloonix_evt_peer_flow_control name=%s num=%d tidx=%d rank=%d stop=%d",
  name, num, tidx, rank, stop);
  rpct_send_evt_msg(ptr, llid, 0, evt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_init(void *ptr, t_rpct_tx rpc_tx)
{
  t_rpct_ctx *ctx;
  t_all_ctx_head *ctx_head;
  if (!ptr)
    {
    g_rpct_ctx = (t_rpct_ctx *) malloc(sizeof(t_rpct_ctx));
    ctx = g_rpct_ctx;
    }
  else
    {
    ctx_head = (t_all_ctx_head *) ptr;
    ctx_head->rpct_ctx = malloc(sizeof(t_rpct_ctx));
    ctx = (t_rpct_ctx *) ctx_head->rpct_ctx;
    }
  memset(ctx, 0, sizeof(t_rpct_ctx));
  ctx->g_buf_tx = (char *) malloc(100000);
  ctx->g_string_tx = rpc_tx;
  ctx->g_pid = (int) getpid();
  extract_boundary(BLKD_ITEM_SUB,  ctx->g_bound_list[bnd_rpct_blkd_item_sub]);
  extract_boundary(BLKD_ITEM,      ctx->g_bound_list[bnd_rpct_blkd_item]);
  extract_boundary(MUEVT_MSG_O,    ctx->g_bound_list[bnd_rpct_evt_msg]);
  extract_boundary(MUAPP_MSG_O,    ctx->g_bound_list[bnd_rpct_app_msg]);
  extract_boundary(MUDIAG_MSG_O,    ctx->g_bound_list[bnd_rpct_diag_msg]);
  extract_boundary(MUCLI_REQ_O,    ctx->g_bound_list[bnd_rpct_cli_req]);
  extract_boundary(MUCLI_RESP_O,   ctx->g_bound_list[bnd_rpct_cli_resp]);
  extract_boundary(HOP_PID_REQ,    ctx->g_bound_list[bnd_rpct_pid_req]);
  extract_boundary(HOP_KIL_REQ,    ctx->g_bound_list[bnd_rpct_kil_req]);
  extract_boundary(HOP_PID_RESP,   ctx->g_bound_list[bnd_rpct_pid_resp]);
  extract_boundary(HOP_EVT_SUB,    ctx->g_bound_list[bnd_rpct_hop_evt_sub]);
  extract_boundary(HOP_EVT_UNSUB,  ctx->g_bound_list[bnd_rpct_hop_evt_unsub]);
  extract_boundary(HOP_EVT_O,      ctx->g_bound_list[bnd_rpct_hop_evt_msg]);
}
/*---------------------------------------------------------------------------*/
