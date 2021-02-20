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

typedef struct t_traf_chain
{
  int llid_unix_traf;
  char name[MAX_NAME_LEN];
  char wlan[MAX_NAME_LEN];
  int num;
  int tidx;
  int nb_pkt_tx;
  int nb_bytes_tx;
  int nb_pkt_rx;
  int nb_bytes_rx;
  struct t_traf_chain *prev;
  struct t_traf_chain *next;
} t_traf_chain; 

static t_traf_chain *g_traf_chain_head;
static int g_llid_unix_sock_traf_nb;

static t_traf_chain *g_tab_traf_chain[CLOWNIX_MAX_CHANNELS];

/*****************************************************************************/
static t_traf_chain *find_with_llid(int llid)
{
  return(g_tab_traf_chain[llid]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_traf_chain *find_with_name(char *name, int num, int tidx)
{
  t_traf_chain *cur = g_traf_chain_head;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && 
        (cur->num == num) && 
        (cur->tidx == tidx))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_traf_chain_exists(char *name, int num)
{
  int result = 0;
  t_traf_chain *cur = g_traf_chain_head;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      {
      result = 1;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_traf_chain_get_llid(char *name, int num)
{
  int result = 0;
  t_traf_chain *cur = g_traf_chain_head;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      {
      result = cur->llid_unix_traf;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_traf_chain_insert(int llid, char *wlan, char *name, int num, int tidx)
{
  int result = -1;
  t_traf_chain *cur = find_with_llid(llid);
  if (cur)
    KERR("%d %s %d", llid, name, num);
  else
    cur = find_with_name(name, num, tidx);
  if (cur)
    KERR("%d %s %d %d", llid, name, num, tidx);
  else if (llid <= 0)
    KERR("%d %s %d %d", llid, name, num, tidx);
  else
    { 
    cur = (t_traf_chain *) malloc(sizeof(t_traf_chain));
    memset(cur, 0, sizeof(t_traf_chain));
    cur->llid_unix_traf = llid;
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    strncpy(cur->wlan, wlan, MAX_NAME_LEN-1);
    cur->num = num;
    cur->tidx = tidx;
    if (g_traf_chain_head)
      g_traf_chain_head->prev = cur;
    cur->next = g_traf_chain_head;
    g_traf_chain_head = cur;
    if (g_tab_traf_chain[llid])
      KOUT(" ");
    g_tab_traf_chain[llid] = cur;
    g_llid_unix_sock_traf_nb += 1;
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_traf_chain_extract_by_llid(int llid)
{
  t_traf_chain *cur = find_with_llid(llid);
  if (!cur)
    KERR("%d", llid);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_traf_chain_head)
      g_traf_chain_head = cur->next;
    free(cur);
    if (!g_tab_traf_chain[llid])
      KOUT(" ");
    g_tab_traf_chain[llid] = NULL;
    g_llid_unix_sock_traf_nb -= 1;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_traf_chain_extract_by_name(char *name, int num, int tidx)
{
  int result = -1;
  t_traf_chain *cur = find_with_name(name, num, tidx);
  if (!cur)
    KERR("%s %d %d", name, num, tidx);
  else
    {
    result = cur->llid_unix_traf;
    if (cur->llid_unix_traf <= 0)
      KOUT("%d %s %d", cur->llid_unix_traf, name, num);
    llid_traf_chain_extract_by_llid(cur->llid_unix_traf);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_traf_get_tab(t_all_ctx *all_ctx, int llid,  int32_t **llid_tab)
{
  t_traf_chain *next, *cur;
  int is_blkd, idx = 0;
  *llid_tab = malloc(g_llid_unix_sock_traf_nb * sizeof(int64_t));
  memset(*llid_tab, 0, g_llid_unix_sock_traf_nb * sizeof(int64_t));
  cur = g_traf_chain_head;
  while (cur)
    {
    next = cur->next;
    if (cur->llid_unix_traf != llid)
      {
      if (msg_exist_channel(all_ctx,cur->llid_unix_traf,&is_blkd,__FUNCTION__))
        {
        if (idx > g_llid_unix_sock_traf_nb)
          KOUT("%d %d", idx, g_llid_unix_sock_traf_nb);
        (*llid_tab)[idx] = cur->llid_unix_traf;
        idx += 1;
        }
      }
    cur = next;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_traf_endp_rx(int llid, int len)
{
  t_traf_chain *utraf = find_with_llid(llid);
  if (!utraf)
    KERR(" ");
  else
    {
    utraf->nb_pkt_rx += 1;
    utraf->nb_bytes_rx += len;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_traf_endp_tx(int llid, int len)
{
  t_traf_chain *utraf = find_with_llid(llid);
  if (!utraf)
    KERR(" ");
  else
    {
    utraf->nb_pkt_tx += 1;
    utraf->nb_bytes_tx += len;
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void collect_eventfull(t_traf_chain *utraf,
                              int *nb_pkt_tx, int *nb_bytes_tx,
                              int *nb_pkt_rx, int *nb_bytes_rx)
{
  *nb_pkt_tx   = utraf->nb_pkt_tx;
  *nb_bytes_tx = utraf->nb_bytes_tx;
  *nb_pkt_rx   = utraf->nb_pkt_rx;
  *nb_bytes_rx = utraf->nb_bytes_rx;
  utraf->nb_pkt_tx   = 0;
  utraf->nb_bytes_tx = 0;
  utraf->nb_pkt_rx   = 0;
  utraf->nb_bytes_rx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_endp(t_all_ctx *all_ctx, int cloonix_llid,
                           t_traf_chain *utraf)
{
  int nb_pkt_tx, nb_pkt_rx, nb_bytes_tx, nb_bytes_rx;
  char txt[3*MAX_NAME_LEN];
  collect_eventfull(utraf, &nb_pkt_tx, &nb_bytes_tx, &nb_pkt_rx, &nb_bytes_rx);
  memset(txt, 0, 3*MAX_NAME_LEN);
  snprintf(txt, (3*MAX_NAME_LEN) - 1,
           "wlan_eventfull_tx_rx %u %s %d %d %s %d %d %d %d",
                           (unsigned int) cloonix_get_msec(),
                           utraf->name, utraf->num, utraf->tidx, utraf->wlan,
                           nb_pkt_tx, nb_bytes_tx, nb_pkt_rx, nb_bytes_rx);
  rpct_send_evt_msg(all_ctx, cloonix_llid, 0, txt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_can_be_sent(t_all_ctx *all_ctx, void *data)
{
  int llid = blkd_get_cloonix_llid((void *) all_ctx);
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  t_traf_chain *cur = g_traf_chain_head;
  if (cidx)
    {
    while (cur)
      {
      eventfull_endp(all_ctx, llid, cur);
      cur = cur->next;
      }
    }
  clownix_timeout_add(all_ctx, 5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_traf_init(t_all_ctx *all_ctx)
{
  g_traf_chain_head = NULL;
  g_llid_unix_sock_traf_nb = 0;
  memset(g_tab_traf_chain, 0, CLOWNIX_MAX_CHANNELS*sizeof(t_traf_chain *));
  clownix_timeout_add(all_ctx, 500, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/


