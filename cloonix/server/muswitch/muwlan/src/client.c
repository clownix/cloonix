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
#include <signal.h>
#include <sys/types.h>
#include <libgen.h>
#include <stdint.h>

#include "ioc.h"
#include "llid_traf.h"
 
void linker_helper1_fct(void)
{
printf("useless");
}


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
  DOUT((void *) all_ctx, FLAG_HOP_DIAG, "Hello from wlan %s", all_ctx->g_name);
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

/****************************************************************************/
static void process_blkd_tx(void *ptr, int llid, int len, char *payload)
{
  t_blkd *bd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, llid_tab[1];
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    {
    llid_tab[0] = llid;
    bd = blkd_create_tx_empty(0,0,0);
    memcpy(bd->payload_blkd, payload, len);
    bd->payload_len = len;
    blkd_put_tx((void *) all_ctx, 1, llid_tab, bd);
    }
  else
    {
    KERR("%d", len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void msg_to_guest(void *ptr, int llid, char *name, int num, 
                         int srv_llid, int srv_tid, int cli_llid,
                         int cli_tid, int len, char *msg)
{
  t_header_wlan *header_wlan;
  int req_len;
  char prefix[MAX_PATH_LEN];
  memset(prefix, 0, MAX_PATH_LEN);
  snprintf(prefix, MAX_PATH_LEN-1, WLAN_HEADER_FORMAT,
           name, num, srv_llid, srv_tid, cli_llid, cli_tid);
  req_len = sizeof(t_header_wlan) + len + strlen(prefix) + 1;
  header_wlan = (t_header_wlan *) malloc(req_len);
  memset(header_wlan, 0, req_len);
  header_wlan->wlan_header_type = wlan_header_type_guest_host;
  header_wlan->data_len = len + strlen(prefix) + 1;
  snprintf(header_wlan->data, len + strlen(prefix), "%s%s", prefix, msg);
  process_blkd_tx(ptr, llid, req_len, (char *) header_wlan);
  free(header_wlan);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  char resp[2*MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  int traf_llid, num, len = strlen(line);
  memset(resp, 0, 2*MAX_PATH_LEN);
  if (len < MAX_PATH_LEN)
    {
    if (sscanf(line, "name=%s num=%d %s", name, &num, resp) == 3) 
      {  
      traf_llid = llid_traf_chain_get_llid(name, num);
      if (traf_llid > 0)
        msg_to_guest(ptr, traf_llid, name, num, llid, tid, 
                     cli_llid, cli_tid, strlen(resp)+1, resp);
      else
        KERR("name=%s num=%d not connected", name, num);
      }
    else
      {
      memset(resp, 0, 2*MAX_PATH_LEN);
      snprintf(resp, 2*MAX_PATH_LEN -1, "WLAN CLI RX BAD FORMAT: %s", line);
      rpct_send_cli_resp(ptr, llid, tid, cli_llid, cli_tid, resp);
      }
    }
  else
    {
    snprintf(resp, 2*MAX_PATH_LEN -1, "WLAN CLI RX TOO LONG MSG: %d", len);
    rpct_send_cli_resp(ptr, llid, tid, cli_llid, cli_tid, resp);
    }
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

