/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "llid_traf.h"


/*****************************************************************************/
static void err_traf_cb (void *ptr, int llid, int err, int from)
{
  int is_blkd; 
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  llid_traf_chain_extract_by_llid(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int blkd_wlan_type(t_blkd *blkd, int *must_be_counted, char **data)
{
  int result = 0;
  t_header_wlan *hwlan = (t_header_wlan *) blkd->payload_blkd;
  struct ieee80211_hdr *hdr = (struct ieee80211_hdr *) hwlan->data;
  *must_be_counted = 0;
  *data = hwlan->data;
  if (hwlan->wlan_header_type == wlan_header_type_peer_data)
    {
    hwlan = (t_header_wlan *) blkd->payload_blkd;
    hdr = (struct ieee80211_hdr *) hwlan->data;
    if (hdr->frame_control[1] != 0)
      *must_be_counted = 1;
    result = wlan_header_type_peer_data;
    }
  else if (hwlan->wlan_header_type == wlan_header_type_peer_sig_addr)
    {
    if (blkd->payload_len != sizeof(t_header_wlan))
      KERR("%d %d", blkd->payload_len, (int)sizeof(t_header_wlan));
    result = wlan_header_type_peer_sig_addr;
    }
  else if (hwlan->wlan_header_type == wlan_header_type_guest_host)
    {
    result = wlan_header_type_guest_host;
    KERR("%s", hwlan->data);
    }
  else
    {
    KERR("%d", hwlan->wlan_header_type);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void msg_from_guest(char *ptr, char *msg)
{
  int num, cli_llid, cli_tid, srv_llid, srv_tid;
  char name[MAX_NAME_LEN];
  char *rsp;
    KERR("%s", msg);
  if (sscanf(msg, WLAN_HEADER_FORMAT, name, &num,
                                      &srv_llid, &srv_tid,
                                      &cli_llid, &cli_tid) != 6)
    KERR("%s", msg);
  else if (!strstr(msg, "end_wlan_head"))
    KERR("%s", msg);
  else
    {
    rsp = strstr(msg, "end_wlan_head")+strlen("end_wlan_head");
    rpct_send_cli_resp(ptr, srv_llid, srv_tid, cli_llid, cli_tid, rsp);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_traf_cb(void *ptr, int llid)
{
  t_blkd *blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int i, nb_llid, wlan_type, must_be_counted;
  int32_t *llid_tab;
  char *data;
  nb_llid = llid_traf_get_tab(all_ctx, llid, &llid_tab);
  blkd = blkd_get_rx(ptr, llid);
  while(blkd)
    {
    wlan_type = blkd_wlan_type(blkd, &must_be_counted, &data);
    if ((wlan_type == wlan_header_type_peer_data) ||
        (wlan_type == wlan_header_type_peer_sig_addr))
      {
      if (must_be_counted)
        llid_traf_endp_tx(llid, blkd->payload_len);
      if (nb_llid > 0)
        {
        if ((blkd->payload_len > PAYLOAD_BLKD_SIZE) || 
            (blkd->payload_len <=0))
          {
          KERR("%d %d", (int) PAYLOAD_BLKD_SIZE, blkd->payload_len);
          blkd_free(ptr, blkd);
          }
        else
          {
          blkd_put_tx(ptr, nb_llid, llid_tab, blkd);
          if (must_be_counted)
            {
            for (i=0; i<nb_llid; i++)
              llid_traf_endp_rx(llid_tab[i], blkd->payload_len);
            }
          }
        }
      else
        blkd_free(ptr, blkd);
      }
    else
      {
      if (wlan_type == wlan_header_type_guest_host)
        {
        msg_from_guest(ptr, data);
        }
      else
        {
        KERR("%d", wlan_type);
        }
      blkd_free(ptr, blkd);
      }
    blkd = blkd_get_rx(ptr, llid);
    }
  free(llid_tab);
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
  clownix_timeout_add(all_ctx,100,fct_timeout_self_destroy,NULL,NULL,NULL);
  all_ctx->g_self_destroy_requested = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_connect_req(void *ptr, char *resp, char *wlan,
                                char *sock_path,
                                char *name, int num, int tidx) 
{
  int newllid;
  if (llid_traf_chain_exists(name, num))
    {
    KERR("%s %d", name, num);
    snprintf(resp, MAX_RESP_LEN-1,
             "cloonix_resp_connect_ko=%s name=%s num=%d tidx=%d",
             sock_path, name, num, tidx);
    }
  else
    {
    newllid = blkd_client_connect(ptr, sock_path, rx_traf_cb, err_traf_cb);
    if (newllid == 0)
      {
      KERR("%s %d", name, num);
      snprintf(resp, MAX_RESP_LEN-1,
               "cloonix_resp_connect_ko=%s name=%s num=%d tidx=%d",
               sock_path, name, num, tidx);
      }
    else
      {
      if (!llid_traf_chain_insert(newllid, wlan, name, num, tidx))
        snprintf(resp, MAX_RESP_LEN-1,
                 "cloonix_resp_connect_ok=%s name=%s num=%d tidx=%d",
                 sock_path, name, num, tidx);
      else
        {
        KERR("%s %d", name, num);
        snprintf(resp, MAX_RESP_LEN-1,
                 "cloonix_resp_connect_ko=%s name=%s num=%d tidx=%d",
                 sock_path, name, num, tidx);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_disconnect_req(void *ptr, char *resp,
                                   char *name, int num, int tidx)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, llid;
  if (!llid_traf_chain_exists(name, num))
    {
    snprintf(resp, MAX_RESP_LEN-1,
             "cloonix_resp_disconnect_ko name=%s num=%d tidx=%d",
             name, num, tidx);
    }
  else
    {  
    llid = llid_traf_chain_extract_by_name(name, num, tidx);
    if (llid > 0)
      {
      snprintf(resp, MAX_RESP_LEN-1,
               "cloonix_resp_disconnect_ok name=%s num=%d tidx=%d",
               name, num, tidx);
      if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
        msg_delete_channel(all_ctx, llid);
      }
    else
      {
      KERR("%s %d", name, num);
      snprintf(resp, MAX_RESP_LEN-1,
               "cloonix_resp_disconnect_ko name=%s num=%d tidx=%d",
               name, num, tidx);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char resp[MAX_RESP_LEN];
  char sock_path[MAX_PATH_LEN];
  char wlan[MAX_NAME_LEN];
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
                  "cloonix_req_connect=%s wlan=%s name=%s num=%d tidx=%d",
                   sock_path, wlan, name, &num, &tidx) == 5)
    {
    if (strcmp(wlan, all_ctx->g_name))
      KOUT("%s %s", wlan, all_ctx->g_name);
    process_connect_req(ptr, resp, wlan, sock_path, name, num, tidx); 
    }
  else if (sscanf(line, 
                  "cloonix_req_disconnect wlan=%s name=%s num=%d tidx=%d",
                  wlan, name, &num, &tidx) == 4)
    {
    if (strcmp(wlan, all_ctx->g_name))
      KOUT("%s %s", wlan, all_ctx->g_name);
    process_disconnect_req(ptr, resp, name, num, tidx);
    }
  else if (sscanf(line, "muend_req_handshake lan=%s name=%s num=%d tidx=%d",
                  wlan, name, &num, &tidx) == 4)
    {
    KERR("%s", line);
    snprintf(resp, MAX_RESP_LEN-1,
             "muend_resp_rank_alloc_ko lan=%s name=%s num=%d tidx=%d",
             wlan, name, num, tidx);
    }
  else
    {
    KERR("%s", line);
    }
  if (strncmp(resp, "KO", 2))
    {
    DOUT((void *) all_ctx, FLAG_HOP_DIAG, "%s", resp);
    rpct_send_diag_msg((void *) all_ctx, llid, tid, resp);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{ 
  DOUT(ptr, FLAG_HOP_EVT, "%s", line);
  KERR("%s", line);
}
/*---------------------------------------------------------------------------*/



